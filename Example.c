// Example of using the C API of b to compile simple C
// programs.  Offers little dependency management; changing
// header files or editing the target list do not cause
// recompilation.  I consider this a bug, but rule maps
// aren't implemented yet.  (Hell, I haven't even tried
// compiling any of this project yet!)

#include "BuildContext.h"
#include "DatabaseInMemory.h"
#include "Exception.h"
#include "FileQuestion.h"
#include "FileRule.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *
c_object_files[] = {
    "Answer.c.o",
    "BuildContext.c.o",
    "Example.c.o",
    "Question.c.o",
};

static const size_t
c_object_files_count = sizeof(c_object_files) / sizeof(*c_object_files);

static const char *
cc_object_files[] = {
    "DatabaseInMemory.cc.o",
};

static const size_t
cc_object_files_count = sizeof(cc_object_files) / sizeof(*cc_object_files);

static const char *
output_file = "b-cc-example";

void
run_command(
    const char *const args[],
    struct B_Exception **ex) {
    pid_t child_pid = fork();
    if (child_pid == 0) {
        // Child.
        execvp(args[0], (char *const *) args);
        perror(NULL);
        exit(1);
    } else if (child_pid > 0) {
        // Parent.
        int status;
        int err = waitpid(child_pid, &status, 0);
        if (err) {
            *ex = b_exception_constant_string("Failed to waitpid while running command");
        } else if (status) {
            *ex = b_exception_constant_string("Command exited with nonzero exit code");
        } else {
            *ex = 0;
        }
    } else {
        *ex = b_exception_constant_string("Failed to fork while running command");
    }
}

static char *
drop_extension(
    const char *path) {
    size_t size = strlen(path) + 1;
    char *s = malloc(size);
    memcpy(s, path, size);

    char *dot = strrchr(s, '.');
    if (dot) {
        *dot = '\0';
    }
    return s;
}

static void
run_c_compile(
    struct B_BuildContext *ctx,
    const char *object_path,
    struct B_Exception **ex) {
    char *c_path = drop_extension(object_path);
    run_command((const char *[]) {
        "clang",
        "-o", object_path,
        "-c", c_path,
        NULL,
    }, ex);
    free(c_path);
}

static void
run_cc_compile(
    struct B_BuildContext *ctx,
    const char *object_path,
    struct B_Exception **ex) {
    char *cc_path = drop_extension(object_path);
    run_command((const char *[]) {
        "clang++",
        "-std=c++11",
        "-o", object_path,
        "-c", cc_path,
        NULL,
    }, ex);
    free(cc_path);
}

static void
run_cc_link(
    struct B_BuildContext *ctx,
    const char *output_path,
    struct B_Exception **ex) {
    // What is two lines in other languages:
    //
    // > ["clang++", "-o", output_path]
    // > ++ c_object_files ++ cc_object_files
    //
    // Is over ten lines in C99.

    const char *static_args[] = {
        "clang++", "-o", output_path,
    };
    size_t static_args_count = sizeof(static_args) / sizeof(*static_args);

    size_t object_files_count = c_object_files_count + cc_object_files_count;
    size_t args_count = object_files_count + static_args_count + 1;
    const char *args[args_count];

    size_t arg = 0;
    for (size_t i = 0; i < static_args_count; ++i, ++arg) {
        args[arg] = static_args[i];
    }
    for (size_t i = 0; i < c_object_files_count; ++i, ++arg) {
        args[arg] = c_object_files[i];
    }
    for (size_t i = 0; i < cc_object_files_count; ++i, ++arg) {
        args[arg] = cc_object_files[i];
    }
    args[arg] = NULL;

    run_command(args, ex);
}

int
main(int argc, char **argv) {
    int err = 0;

    struct B_AnyDatabase *database
        = b_database_in_memory_allocate();
    const struct B_DatabaseVTable *database_vtable
        = b_database_in_memory_vtable();

    struct B_AnyRule *rule
        = b_file_rule_allocate();
    const struct B_RuleVTable *rule_vtable
        = b_file_rule_vtable();

    b_file_rule_add_many(
        rule,
        c_object_files,
        c_object_files_count,
        run_c_compile);
    b_file_rule_add_many(
        rule,
        cc_object_files,
        cc_object_files_count,
        run_cc_compile);
    b_file_rule_add(
        rule,
        output_file,
        run_cc_link);

    struct B_AnyQuestion *question
        = b_file_question_constant_string(output_file);
    const struct B_QuestionVTable *question_vtable
        = b_file_question_constant_string_vtable();

    struct B_BuildContext *ctx
        = b_build_context_allocate(
            database,
            database_vtable,
            rule,
            rule_vtable);

    struct B_Exception *ex = NULL;

    b_build_context_need_one(
        ctx,
        question,
        question_vtable,
        &ex);

    if (ex) {
        fprintf(
            stderr,
            "Exception occured while building:\n  %s\n",
            ex->message);
        err = 1;
    }

    b_build_context_deallocate(ctx);
    b_file_rule_deallocate(rule);
    b_database_in_memory_deallocate(database);

    return err;
}
