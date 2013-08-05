#include "Answer.h"
#include "Database.h"
#include "DatabaseInMemory.h"
#include "Question.h"
#include "Serialize.h"

#include <algorithm>
#include <cassert>
#include <list>
#include <map>
#include <memory>

// Serialization formats:
//
// DatabaseInMemory:
//
// - size_t question_answer_count;
// - QuestionAnswer question_answers[question_answer_count];
//
// - size_t dependency_count;
// - Dependency dependencies[dependency_count];
//
// QuestionAnswer:
//
// - size_t struct_size;
//
// - UUID question_vtable_uuid;
//
// - size_t question_size;
// - uint8_t question[question_size];
// - size_t answer_size;
// - uint8_t answer[answer_size];
//
// Dependency:
//
// - size_t struct_size;
//
// - UUID from_vtable_uuid;
// - UUID to_vtable_uuid;
//
// - size_t from_size;
// - uint8_t from[from_size];
// - size_t to_size;
// - uint8_t to[to_size];
//
// UUID:
//
// - size_t uuid_size;
// - uint8_t uuid[uuid_size];

static void
serialize_uuid(
    B_UUID uuid,
    B_Serializer s,
    void *c) {
    size_t size = strlen(uuid.uuid);
    b_serialize_size_t(size, s, c);
    s(uuid.uuid, size, c);
}

struct QuestionAnswer {
    B_AnyQuestion *question;
    const B_QuestionVTable *question_vtable;
    B_AnyAnswer *answer;

    QuestionAnswer(
        const B_AnyQuestion *question,
        const B_QuestionVTable *question_vtable,
        const B_AnyAnswer *answer) :
        question(question_vtable->replicate(question)),
        question_vtable(question_vtable),
        answer(question_vtable->answer_vtable->replicate(answer)) {
    }

    ~QuestionAnswer() {
        this->question_vtable
            ->deallocate(this->question);
        this->question_vtable->answer_vtable
            ->deallocate(this->answer);
    }

    void
    serialize(
        B_Serializer s,
        void *c) const {
        serialize_uuid(
            this->question_vtable->uuid,
            s,
            c);
        b_serialize_sized(
            this->question,
            b_serialize_size_t,
            this->question_vtable->serialize,
            s,
            c);
        b_serialize_sized(
            this->answer,
            b_serialize_size_t,
            this->question_vtable->answer_vtable->serialize,
            s,
            c);
    }
};

struct Dependency {
    B_AnyQuestion *from;
    const B_QuestionVTable *from_vtable;
    B_AnyQuestion *to;
    const B_QuestionVTable *to_vtable;

    Dependency(
        const B_AnyQuestion *from,
        const B_QuestionVTable *from_vtable,
        const B_AnyQuestion *to,
        const B_QuestionVTable *to_vtable) :
        from(from_vtable->replicate(from)),
        from_vtable(from_vtable),
        to(to_vtable->replicate(to)),
        to_vtable(to_vtable) {
    }

    ~Dependency() {
        this->from_vtable->deallocate(this->from);
        this->to_vtable->deallocate(this->to);
    }

    void
    serialize(
        B_Serializer s,
        void *c) const {
        serialize_uuid(
            this->from_vtable->uuid,
            s,
            c);
        serialize_uuid(
            this->to_vtable->uuid,
            s,
            c);
        b_serialize_sized(
            this->from,
            b_serialize_size_t,
            this->from_vtable->serialize,
            s,
            c);
        b_serialize_sized(
            this->to,
            b_serialize_size_t,
            this->to_vtable->serialize,
            s,
            c);
    }
};

struct UnresolvedQuestionAnswer {
    B_UUID question_vtable_uuid;

    const char *question_data;
    size_t question_data_size;

    const char *answer_data;
    size_t answer_data_size;

    static std::unique_ptr<UnresolvedQuestionAnswer>
    deserialize(
        B_Deserializer s,
        void *c) {
        // TODO
        assert(0);
    }
};

struct UnresolvedDependency {
    B_UUID from_vtable_uuid;
    const char *from_data;
    size_t from_data_size;

    B_UUID to_vtable_uuid;
    const char *to_data;
    size_t to_data_size;

    static std::unique_ptr<UnresolvedDependency>
    deserialize(
        B_Deserializer s,
        void *c) {
        // TODO
        assert(0);
    }
};

struct DatabaseInMemory {
    std::list<QuestionAnswer> question_answers;
    std::list<Dependency> dependencies;

    std::map<B_UUID, B_QuestionVTable *> question_vtables;

    std::map<B_UUID, UnresolvedQuestionAnswer> unresolved_question_answers;
    std::map<B_UUID, std::shared_ptr<UnresolvedDependency>> unresolved_dependencies;

    void
    add_dependency(
        const B_AnyQuestion *from,
        const B_QuestionVTable *from_vtable,
        const B_AnyQuestion *to,
        const B_QuestionVTable *to_vtable,
        B_Exception **ex) {
        this->dependencies.emplace_back(
            from,
            from_vtable,
            to,
            to_vtable);
    }

    struct B_AnyAnswer *
    get_answer(
        const struct B_AnyQuestion *question,
        const struct B_QuestionVTable *question_vtable,
        struct B_Exception **ex) {
        for (const auto &qa : this->question_answers) {
            if (qa.question_vtable == question_vtable
            && question_vtable->equal(question, qa.question)) {
                return question_vtable->answer_vtable
                    ->replicate(qa.answer);
            }
        }
        return nullptr;
    }

    void
    set_answer(
        const struct B_AnyQuestion *question,
        const struct B_QuestionVTable *question_vtable,
        const struct B_AnyAnswer *answer,
        struct B_Exception **ex) {
        for (auto &qa : this->question_answers) {
            if (qa.question_vtable == question_vtable
            && question_vtable->equal(question, qa.question)) {
                question_vtable->answer_vtable
                    ->deallocate(qa.answer);
                qa.answer = question_vtable->answer_vtable
                    ->replicate(answer);
                return;
            }
        }

        this->question_answers.emplace_back(
            question,
            question_vtable,
            answer);
    }

    void
    serialize(
        B_Serializer s,
        void *c) const {
        b_serialize_size_t(this->question_answers.size(), s, c);
        for (const QuestionAnswer &qa : this->question_answers) {
            b_serialize_sized(qa, b_serialize_size_t, s, c);
        }

        b_serialize_size_t(this->dependencies.size(), s, c);
        for (const Dependency &dep : this->dependencies) {
            b_serialize_sized(dep, b_serialize_size_t, s, c);
        }
    }

    static std::unique_ptr<DatabaseInMemory>
    deserialize(
        B_Deserializer s,
        void *c) {
        bool ok;
        auto db = std::unique_ptr<DatabaseInMemory>(
            new DatabaseInMemory());

        size_t qa_count = b_deserialize_size_t(&ok, s, c);
        if (!ok) return nullptr;
        for (size_t i = 0; i < qa_count; ++i) {
            std::unique_ptr<UnresolvedQuestionAnswer> qa
                = b_deserialize_sized<UnresolvedQuestionAnswer>(
                        b_deserialize_size_t,
                        s,
                        c);
            if (!qa) return nullptr;
            db->unresolved_question_answers.emplace(
                qa->question_vtable_uuid,
                *qa);
        }

        size_t dep_count = b_deserialize_size_t(&ok, s, c);
        if (!ok) return nullptr;
        for (size_t i = 0; i < dep_count; ++i) {
            auto dep = std::shared_ptr<UnresolvedDependency>(
                b_deserialize_sized<UnresolvedDependency>(
                    b_deserialize_size_t,
                    s,
                    c).release());
            if (!dep) return nullptr;
            db->unresolved_dependencies.emplace(
                dep->from_vtable_uuid,
                dep);
            db->unresolved_dependencies.emplace(
                dep->to_vtable_uuid,
                dep);
        }

        return db;
    }
};

static B_AnyDatabase *
cast(DatabaseInMemory *database) {
    return reinterpret_cast<B_AnyDatabase *>(database);
}

static DatabaseInMemory *
cast(B_AnyDatabase *database) {
    return reinterpret_cast<DatabaseInMemory *>(database);
}

B_AnyDatabase *
b_database_in_memory_allocate() {
    return cast(new DatabaseInMemory());
};

void
b_database_in_memory_deallocate(
    B_AnyDatabase *database
) {
    delete cast(database);
}

const B_DatabaseVTable *
b_database_in_memory_vtable() {
    static const B_DatabaseVTable vtable = {
        .deallocate = b_database_in_memory_deallocate,

        .add_dependency = [](
            B_AnyDatabase *database,
            const B_AnyQuestion *from,
            const B_QuestionVTable *from_vtable,
            const B_AnyQuestion *to,
            const B_QuestionVTable *to_vtable,
            B_Exception **ex) {
            cast(database)->add_dependency(
                from,
                from_vtable,
                to,
                to_vtable,
                ex);
        },

        .get_answer = [](
            B_AnyDatabase *database,
            const B_AnyQuestion *question,
            const B_QuestionVTable *question_vtable,
            B_Exception **ex) -> B_AnyAnswer *{
            return cast(database)->get_answer(
                question,
                question_vtable,
                ex);
        },

        .set_answer = [](
            B_AnyDatabase *database,
            const B_AnyQuestion *question,
            const B_QuestionVTable *question_vtable,
            const B_AnyAnswer *answer,
            B_Exception **ex) {
            return cast(database)->set_answer(
                question,
                question_vtable,
                answer,
                ex);
        },
    };
    return &vtable;
}

void
b_database_in_memory_serialize(
    const void *database,
    B_Serializer s,
    void *c) {
    b_serialize<DatabaseInMemory>(database, s, c);
}

void *
b_database_in_memory_deserialize(
    B_Deserializer s,
    void *c) {
    return b_deserialize<DatabaseInMemory>(s, c).release();
}