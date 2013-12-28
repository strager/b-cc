#include <B/FileQuestion.h>
#include <B/FileRule.h>
#include <B/Internal/Allocate.h>
#include <B/Question.h>
#include <B/Rule.h>
#include <B/UUID.h>

#include <sys/stat.h>

struct FileRuleNode {
    struct B_AnyQuestion *question;
    B_FileRuleCallback callback;
    struct FileRuleNode *next;  // Nullable.
};

struct FileRule {
    struct FileRuleNode *head;  // Nullable.
};

struct B_AnyRule *
b_file_rule_allocate(void) {
    B_ALLOCATE(struct FileRule, rule, {
        .head = NULL,
    });
    return (struct B_AnyRule *) rule;
}

static void
b_file_rule_deallocate_node(
    struct FileRuleNode *node) {
    if (!node) {
        return;
    }
    b_file_rule_deallocate_node(node->next);
    b_file_question_deallocate(node->question);
    free(node);
}

void
b_file_rule_deallocate(
    struct B_AnyRule *rule_raw) {
    struct FileRule *rule = (struct FileRule *) rule_raw;
    b_file_rule_deallocate_node(rule->head);
    free(rule);
}

void
b_file_rule_add(
    struct B_AnyRule *rule_raw,
    const char *file_path,
    B_FileRuleCallback callback) {
    struct FileRule *rule = (struct FileRule *) rule_raw;
    B_ALLOCATE(struct FileRuleNode, node, {
        .question = b_file_question_allocate(file_path),
        .callback = callback,
        .next = rule->head,
    });
    rule->head = node;
}

void
b_file_rule_add_many(
    struct B_AnyRule *rule,
    const char **file_path,
    size_t count,
    B_FileRuleCallback callback) {
    for (size_t i = 0; i < count; ++i) {
        b_file_rule_add(rule, file_path[i], callback);
    }
}

static void
b_file_rule_combine(
    struct B_AnyRule *rule,
    const struct B_AnyRule *other) {
    for (
        const struct FileRuleNode *node
            = ((const struct FileRule *) other)->head;
        node;
        node = node->next) {
        b_file_rule_add(
            rule,
            b_file_question_file_path(node->question),
            node->callback);
    }
}

static void
b_file_rule_query_callback(
    struct B_BuildContext *ctx,
    const struct B_AnyQuestion *question,
    void *closure,
    struct B_Exception **ex) {
    B_FileRuleCallback callback = (B_FileRuleCallback) closure;
    const char *file_path = b_file_question_file_path(question);
    callback(ctx, file_path, ex);
}

static void
b_file_rule_query_no_callback(
    struct B_BuildContext *ctx,
    const struct B_AnyQuestion *question,
    void *closure,
    struct B_Exception **ex) {
    (void) ctx;
    (void) question;
    (void) closure;
    (void) ex;
    // Do nothing.
}

static void *
b_file_rule_query_replicate_closure(
    const void *closure) {
    return (void *) closure;
}

static void
b_file_rule_query_deallocate_closure(
    void *_closure) {
    // Do nothing.
}

static void
b_file_rule_query(
    const struct B_AnyRule *rule,
    const struct B_AnyQuestion *question,
    const struct B_QuestionVTable *question_vtable,
    const struct B_BuildContext *_ctx,
    struct B_RuleQueryList *list,
    struct B_Exception **_ex) {
    if (!b_uuid_equal(question_vtable->uuid, b_file_question_vtable()->uuid)) {
        return;
    }

    bool added_query = false;
    for (
        const struct FileRuleNode *node
            = ((const struct FileRule *) rule)->head;
        node;
        node = node->next) {
        if (question_vtable->equal(question, node->question)) {
            struct B_RuleQuery query = {
                .function = b_file_rule_query_callback,
                .closure = (void *) node->callback,
                .replicate_closure = b_file_rule_query_replicate_closure,
                .deallocate_closure = b_file_rule_query_deallocate_closure,
            };
            b_rule_query_list_add(list, &query);
            added_query = true;
        }
    }

    if (!added_query) {
        struct stat stat;
        const char *file_path
            = b_file_question_file_path(question);
        int err = lstat(file_path, &stat);
        if (!err) {
            struct B_RuleQuery query = {
                .function = b_file_rule_query_no_callback,
                .closure = NULL,
                .replicate_closure = b_file_rule_query_replicate_closure,
                .deallocate_closure = b_file_rule_query_deallocate_closure,
            };
            b_rule_query_list_add(list, &query);
        }
    }
}

const struct B_RuleVTable *
b_file_rule_vtable(void) {
    static const struct B_RuleVTable vtable = {
        .uuid = B_UUID("3325547E-38B3-4121-BE5A-5E65FEB42C12"),
        .add = b_file_rule_combine,
        .query = b_file_rule_query,
        .deallocate = b_file_rule_deallocate,
    };
    return &vtable;
}
