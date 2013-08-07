// Example of using the C API of b to compile simple C
// programs.  Offers little dependency management; changing
// header files or editing the target list do not cause
// recompilation.  I consider this a bug, but rule maps
// aren't implemented yet.  (Hell, I haven't even tried
// compiling any of this project yet!)

#include "BuildContext.h"
#include "Database.h"
#include "DatabaseInMemory.h"
#include "Exception.h"
#include "FileQuestion.h"
#include "FileRule.h"
#include "Portable.h"
#include "QuestionVTableList.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define DATABASE_FILE_PATH "build.database"

static const char *
c_object_files[] = {
    "lib/Answer.c.o",
    "lib/BuildContext.c.o",
    "lib/Database.c.o",
    "lib/Exception.c.o",
    "lib/FileQuestion.c.o",
    "lib/FileRule.c.o",
    "lib/Portable.c.o",
    "lib/Question.c.o",
    "lib/QuestionVTableList.c.o",
    "lib/Rule.c.o",
    "lib/RuleQueryList.c.o",
    "lib/Serialize.c.o",
    "lib/UUID.c.o",
    "example/Example.c.o",
};

static const size_t
c_object_files_count = sizeof(c_object_files) / sizeof(*c_object_files);

static const char *
cc_object_files[] = {
    "lib/DatabaseInMemory.cc.o",
};

static const size_t
cc_object_files_count = sizeof(cc_object_files) / sizeof(*cc_object_files);

static const size_t
object_files_count = c_object_files_count + cc_object_files_count;

static const char *
output_file = "b-cc-example";

void
print_command(
    FILE *stream,
    const char *const args[]) {
    for (const char *const *arg = args; *arg; ++arg) {
        if (arg != args) {
            fprintf(stream, " ");
        }
        size_t spanned = strcspn(*arg, " \'\"");
        if (spanned == strlen(*arg)) {
            fprintf(stream, "%s", *arg);
        } else {
            fprintf(stream, "'%s'", *arg);
        }
    }
}

void
run_command(
    const char *const args[],
    struct B_Exception **ex) {
    fprintf(stderr, "$ ");
    print_command(stderr, args);
    fprintf(stderr, "\n");

    pid_t child_pid = fork();
    if (child_pid == 0) {
        // Child.
        execvp(args[0], (char *const *) args);
        perror(NULL);
        exit(1);
    } else if (child_pid > 0) {
        // Parent.
        int status;
        int err;
retry_wait:
        err = waitpid(child_pid, &status, 0);
        if (err < 0) {
            if (errno == EINTR) {
                goto retry_wait;
            }
            *ex = b_exception_errno("waitpid", errno);
        } else if (status) {
            *ex = b_exception_string("Command exited with nonzero exit code");
        } else {
            *ex = 0;
        }
    } else {
        *ex = b_exception_string("Failed to fork while running command");
    }
}

static char *
drop_extension(
    const char *path) {
    char *s = b_strdup(path);
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

    struct B_AnyQuestion *question
        = b_file_question_allocate(c_path);
    b_build_context_need_one(
        ctx,
        question,
        b_file_question_vtable(),
        ex);
    b_file_question_deallocate(question);
    B_EXCEPTION_THEN(ex, {
        return;
    });

    run_command((const char *[]) {
        "clang",
        "-Ilib",
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

    struct B_AnyQuestion *question
        = b_file_question_allocate(cc_path);
    b_build_context_need_one(
        ctx,
        question,
        b_file_question_vtable(),
        ex);
    b_file_question_deallocate(question);
    B_EXCEPTION_THEN(ex, {
        return;
    });

    run_command((const char *[]) {
        "clang++",
        "-Ilib",
        "-std=c++11",
        "-stdlib=libc++",
        "-o", object_path,
        "-c", cc_path,
        NULL,
    }, ex);
    free(cc_path);
}

static void
depend_upon_object_files(
    struct B_BuildContext *ctx,
    struct B_Exception **ex) {
    struct B_AnyQuestion *questions[object_files_count];
    const struct B_QuestionVTable *question_vtables[object_files_count];

    {
        const struct B_QuestionVTable *question_vtable
            = b_file_question_vtable();
        size_t question = 0;
        for (size_t i = 0; i < c_object_files_count; ++i, ++question) {
            questions[question]
                = b_file_question_allocate(c_object_files[i]);
            question_vtables[question] = question_vtable;
        }
        for (size_t i = 0; i < cc_object_files_count; ++i, ++question) {
            questions[question]
                = b_file_question_allocate(cc_object_files[i]);
            question_vtables[question] = question_vtable;
        }
    }

    b_build_context_need(
        ctx,
        (const struct B_AnyQuestion *const *) questions,
        question_vtables,
        object_files_count,
        ex);

    for (size_t i = 0; i < object_files_count; ++i) {
        b_file_question_deallocate(questions[i]);
    }
}

static void
run_cc_link(
    struct B_BuildContext *ctx,
    const char *output_path,
    struct B_Exception **ex) {
    depend_upon_object_files(ctx, ex);
    B_EXCEPTION_THEN(ex, {
        return;
    });

    // What is two lines in other languages:
    //
    // > ["clang++", "-stdlib=libc++", "-o", output_path]
    // > ++ c_object_files ++ cc_object_files
    //
    // Is over ten lines in C99.

    const char *static_args[] = {
        "clang++",
        "-stdlib=libc++",
        "-o", output_path,
    };
    size_t static_args_count = sizeof(static_args) / sizeof(*static_args);

    size_t args_count = object_files_count + static_args_count + 1;
    const char *args[args_count];

    {
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
    }

    run_command(args, ex);
}

static struct B_AnyDatabase *
load_database(
    const struct B_QuestionVTableList *question_vtables) {
    int err_code;

    struct stat statbuf;
    err_code = stat(DATABASE_FILE_PATH, &statbuf);
    if (err_code) {
        return b_database_in_memory_allocate();
    }

    void *database;
    err_code = b_deserialize_from_file_path1(
        DATABASE_FILE_PATH,
        &database,
        (B_DeserializeFunc) b_database_in_memory_deserialize,
        (void *) question_vtables);
    if (err_code) {
        perror("Error reading file " DATABASE_FILE_PATH);
        return NULL;
    }
    if (!database) {
        fprintf(stderr, "Error parsing file " DATABASE_FILE_PATH "."
               " Database file is likely corrupt!\n");
        return NULL;
    }

    struct B_Exception *ex = NULL;
    b_database_in_memory_vtable()
        ->recheck_all(database, &ex);
    if (ex) {
        fprintf(
            stderr,
            "Exception occured while rechecking:\n  %s\n",
            ex->message);
        return NULL;
    }

    return database;
}

int
main(int argc, char **argv) {
    int err = 0;

    struct B_QuestionVTableList *question_vtables
        = b_question_vtable_list_allocate();
    b_question_vtable_list_add(
        question_vtables,
        b_file_question_vtable());

    struct B_AnyDatabase *database = load_database(
        question_vtables);
    if (!database) {
        b_question_vtable_list_deallocate(question_vtables);
        return 3;
    }
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
        = b_file_question_allocate(output_file);
    const struct B_QuestionVTable *question_vtable
        = b_file_question_vtable();

    const struct B_BuildContextInfo ctx_info = {
        .database = database,
        .database_vtable = database_vtable,
        .rule = rule,
        .rule_vtable = rule_vtable,
    };
    struct B_BuildContext *ctx
        = b_build_context_allocate(&ctx_info);

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
        // Fall through
    }

    int err_code = b_serialize_to_file_path(
        DATABASE_FILE_PATH,
        database,
        b_database_in_memory_serialize);
    if (err_code) {
        perror("Error writing file " DATABASE_FILE_PATH);
        err = 2;
        // Fall through
    }

    b_question_vtable_list_deallocate(question_vtables);
    b_build_context_deallocate(ctx);
    b_file_rule_deallocate(rule);
    b_database_in_memory_deallocate(database);

    return err;
}
