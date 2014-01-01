// Example of using the C API of b to compile simple C
// programs.  Offers little dependency management; changing
// header files or editing the target list do not cause
// recompilation.  I consider this a bug, but rule maps
// aren't implemented yet.

#include <B/Broker.h>
#include <B/BuildContext.h>
#include <B/Client.h>
#include <B/Database.h>
#include <B/DatabaseInMemory.h>
#include <B/Exception.h>
#include <B/Fiber.h>
#include <B/FileQuestion.h>
#include <B/FileRule.h>
#include <B/Internal/Allocate.h>
#include <B/Internal/Portable.h>
#include <B/Log.h>
#include <B/QuestionVTableList.h>
#include <B/Worker.h>

#include <zmq.h>

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define DATABASE_FILE_PATH "build.database"

static const char *
c_object_files[] = {
    "example/Example2.c.o",
    "lib/src/Answer.c.o",
    "lib/src/Broker.c.o",
    "lib/src/BuildContext.c.o",
    "lib/src/Client.c.o",
    "lib/src/Database.c.o",
    "lib/src/Exception.c.o",
    "lib/src/ExceptionErrno.c.o",
    "lib/src/ExceptionMemory.c.o",
    "lib/src/ExceptionString.c.o",
    "lib/src/Fiber.c.o",
    "lib/src/FileQuestion.c.o",
    "lib/src/FileRule.c.o",
    "lib/src/Internal/Identity.c.o",
    "lib/src/Internal/MessageList.c.o",
    "lib/src/Internal/Portable.c.o",
    "lib/src/Internal/PortableUContext.c.o",
    "lib/src/Internal/Protocol.c.o",
    "lib/src/Internal/Util.c.o",
    "lib/src/Internal/ZMQ.c.o",
    "lib/src/Log.c.o",
    "lib/src/Question.c.o",
    "lib/src/QuestionVTableList.c.o",
    "lib/src/Rule.c.o",
    "lib/src/RuleQueryList.c.o",
    "lib/src/Serialize.c.o",
    "lib/src/UUID.c.o",
    "lib/src/Worker.c.o",
};

static const size_t
c_object_files_count = sizeof(c_object_files) / sizeof(*c_object_files);

static const char *
cc_object_files[] = {
    "lib/src/DatabaseInMemory.cc.o",
    "lib/src/ExceptionAggregate.cc.o",
    "lib/src/ExceptionCXX.cc.o",
};

static const size_t
cc_object_files_count = sizeof(cc_object_files) / sizeof(*cc_object_files);

static const char *
s_object_files[] = {
    "lib/src/Internal/PortableUContextASM.S.o",
};

static const size_t
s_object_files_count = sizeof(s_object_files) / sizeof(*s_object_files);

static const size_t
object_files_count
    = c_object_files_count
    + cc_object_files_count
    + s_object_files_count;

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
    struct B_BuildContext *build_context,
    const char *object_path,
    struct B_Exception **ex) {

    B_LOG(B_INFO, "Compiling %s", object_path);

    char *c_path = drop_extension(object_path);

    struct B_AnyQuestion *question
        = b_file_question_allocate(c_path);
    struct B_Client *client
        = b_build_context_client(build_context);
    *ex = b_client_need_one(
        client,
        question,
        b_file_question_vtable());
    b_file_question_deallocate(question);
    B_EXCEPTION_THEN(ex, {
        return;
    });

    run_command((const char *[]) {
        "cc",
        "-Ilib/include",
        "-o", object_path,
        "-c", c_path,
        NULL,
    }, ex);
    free(c_path);
}

static void
run_cc_compile(
    struct B_BuildContext *build_context,
    const char *object_path,
    struct B_Exception **ex) {

    B_LOG(B_INFO, "Compiling %s", object_path);

    char *cc_path = drop_extension(object_path);

    struct B_AnyQuestion *question
        = b_file_question_allocate(cc_path);
    struct B_Client *client
        = b_build_context_client(build_context);
    *ex = b_client_need_one(
        client,
        question,
        b_file_question_vtable());
    b_file_question_deallocate(question);
    B_EXCEPTION_THEN(ex, {
        return;
    });

    run_command((const char *[]) {
        "c++",
        "-std=c++11",
        "-stdlib=libc++",
        "-Ilib/include",
        "-o", object_path,
        "-c", cc_path,
        NULL,
    }, ex);
    free(cc_path);
}

static void
run_s_compile(
    struct B_BuildContext *build_context,
    const char *object_path,
    struct B_Exception **ex) {

    run_c_compile(build_context, object_path, ex);
}

static void
depend_upon_object_files(
    struct B_Client *client,
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
        for (size_t i = 0; i < s_object_files_count; ++i, ++question) {
            questions[question]
                = b_file_question_allocate(s_object_files[i]);
            question_vtables[question] = question_vtable;
        }
    }

    *ex = b_client_need(
        client,
        (const struct B_AnyQuestion *const *) questions,
        question_vtables,
        object_files_count);

    for (size_t i = 0; i < object_files_count; ++i) {
        b_file_question_deallocate(questions[i]);
    }
}

static void
run_cc_link(
    struct B_BuildContext *build_context,
    const char *output_path,
    struct B_Exception **ex) {

    B_LOG(B_INFO, "Linking %s", output_path);

    struct B_Client *client
        = b_build_context_client(build_context);
    depend_upon_object_files(client, ex);
    B_EXCEPTION_THEN(ex, {
        return;
    });

    // What is two lines in other languages:
    //
    // > ["c++", "-stdlib=libc++", "-lzmq", "-o", output_path]
    // > ++ c_object_files ++ cc_object_files ++ s_object_files
    //
    // Is over ten lines in C99.

    const char *static_args[] = {
        "c++",
        "-stdlib=libc++",
        "-lzmq",
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
        for (size_t i = 0; i < s_object_files_count; ++i, ++arg) {
            args[arg] = s_object_files[i];
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
    b_database_in_memory_recheck_all(database, &ex);
    if (ex) {
        B_LOG_EXCEPTION(ex);
        return NULL;
    }

    return database;
}

struct B_BrokerClosure {
    void *const context_zmq;
    struct B_BrokerAddress const *const broker_address;
    struct B_AnyDatabase *const database;
    struct B_DatabaseVTable const *const database_vtable;
};

static void
broker_thread(
    void *closure_raw) {

    struct B_BrokerClosure closure
        = *(struct B_BrokerClosure *) closure_raw;
    free(closure_raw);

    struct B_Exception *ex = b_broker_run(
        closure.broker_address,
        closure.context_zmq,
        closure.database,
        closure.database_vtable);
    if (ex) {
        B_LOG_EXCEPTION(ex);
        abort();
    }
}

struct B_WorkerClosure {
    void *const context_zmq;
    struct B_BrokerAddress const *const broker_address;
    struct B_QuestionVTableList const *const question_vtables;
    struct B_AnyRule const *const rule;
    struct B_RuleVTable const *const rule_vtable;
};

static void
worker_thread(
    void *closure_raw) {

    struct B_WorkerClosure closure
        = *(struct B_WorkerClosure *) closure_raw;
    free(closure_raw);

    struct B_Exception *ex = b_worker_work(
        closure.context_zmq,
        closure.broker_address,
        closure.question_vtables,
        closure.rule,
        closure.rule_vtable);
    if (ex) {
        B_LOG_EXCEPTION(ex);
        abort();
    }
}

static struct B_Exception *
create_worker_async(
    size_t worker_index,
    void *context_zmq,
    struct B_BrokerAddress const *broker_address,
    struct B_QuestionVTableList const *question_vtables,
    struct B_AnyRule const *rule,
    struct B_RuleVTable const *rule_vtable) {

    B_ALLOCATE(struct B_WorkerClosure, worker_closure, {
        .context_zmq = context_zmq,
        .broker_address = broker_address,
        .question_vtables = question_vtables,
        .rule = rule,
        .rule_vtable = rule_vtable,
    });

    char thread_name[64];
    int rc = snprintf(
        thread_name,
        sizeof(thread_name),
        "worker_%zu",
        worker_index);
    assert(rc != -1);
    assert(rc == strlen(thread_name));

    b_create_thread(
        thread_name,
        worker_thread,
        worker_closure);
    // worker_closure is now owned by the thread.

    return NULL;
}

int
main(
    int argc,
    char **argv) {

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
        b_uuid_deallocate_leaked();
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
    b_file_rule_add_many(
        rule,
        s_object_files,
        s_object_files_count,
        run_s_compile);
    b_file_rule_add(
        rule,
        output_file,
        run_cc_link);

    struct B_AnyQuestion *question
        = b_file_question_allocate(output_file);
    const struct B_QuestionVTable *question_vtable
        = b_file_question_vtable();

    void *context_zmq = zmq_ctx_new();

    struct B_BrokerAddress *broker_address;
    {
        struct B_Exception *ex
            = b_broker_address_allocate(&broker_address);
        if (ex) {
            B_LOG_EXCEPTION(ex);
            abort();  // TODO(strager): Cleanup.
        }
    }

    {
        B_ALLOCATE(struct B_BrokerClosure, broker_closure, {
            .context_zmq = context_zmq,
            .broker_address = broker_address,
            .database = database,
            .database_vtable = database_vtable,
        });

        b_create_thread(
            "broker",
            broker_thread,
            broker_closure);
        // broker_closure is now owned by the thread.
    }

    for (size_t i = 0; i < 1; ++i) {
        struct B_Exception *ex = create_worker_async(
            i,
            context_zmq,
            broker_address,
            question_vtables,
            rule,
            rule_vtable);
        if (ex) {
            B_LOG_EXCEPTION(ex);
            abort();  // TODO(strager): Cleanup.
        }
    }

    struct B_FiberContext *fiber_context;
    {
        struct B_Exception *ex = b_fiber_context_allocate(
            &fiber_context);
        if (ex) {
            B_LOG_EXCEPTION(ex);
            abort();  // TODO(strager): Cleanup.
        }
    }

    struct B_Client *client;
    {
        struct B_Exception *ex = b_client_allocate_connect(
            context_zmq,
            broker_address,
            fiber_context,
            &client);
        if (ex) {
            B_LOG_EXCEPTION(ex);
            abort();  // TODO(strager): Cleanup.
        }
    }

    {
        struct B_Exception *ex = b_client_need_one(
            client,
            question,
            question_vtable);
        if (ex) {
            B_LOG_EXCEPTION(ex);
            err = 1;
            // Fall through
        }
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

    (void) b_client_deallocate_disconnect(client);
    (void) b_fiber_context_deallocate(fiber_context);
    (void) b_broker_address_deallocate(broker_address);
    zmq_ctx_term(context_zmq);
    b_question_vtable_list_deallocate(question_vtables);
    b_file_question_deallocate(question);
    b_file_rule_deallocate(rule);
    b_database_in_memory_deallocate(database);

    b_uuid_deallocate_leaked();

    return err;
}
