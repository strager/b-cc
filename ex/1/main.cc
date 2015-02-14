#include <B/Alloc.h>
#include <B/AnswerContext.h>
#include <B/Assert.h>
#include <B/Database.h>
#include <B/Error.h>
#include <B/File.h>
#include <B/Main.h>
#include <B/Process.h>
#include <B/QuestionAnswer.h>
#include <B/QuestionDispatch.h>
#include <B/QuestionQueue.h>
#include <B/QuestionVTableSet.h>

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <errno.h>
#include <sqlite3.h>
#include <sys/stat.h>

// HACK(strager)
#if defined(__APPLE__)
# define EXTRA_CXXFLAGS "-stdlib=libc++",
#elif defined(__linux__)
# define EXTRA_LDFLAGS "-lpthread", "-ldl",
#endif

#if !defined(EXTRA_CFLAGS)
# define EXTRA_CFLAGS
#endif
#if !defined(EXTRA_CXXFLAGS)
# define EXTRA_CXXFLAGS
#endif
#if !defined(EXTRA_LDFLAGS)
# define EXTRA_LDFLAGS
#endif

static char const *
path_extensions_(
        B_FilePath const *path) {
    // TODO(strager): Support non-UNIX path separators.
    char const *end = strchr(path, '\0');
    char const *slash = strrchr(path, '/');
    size_t file_name_index
        = slash ? slash - path + 1 : 0;
    char const *dot = strchr(path + file_name_index, '.');
    if (!dot) {
        // No extension.
        return end;
    }
    size_t dot_index = dot - path;
    if (dot_index == file_name_index) {
        // Dot file.
        return end;
    } else if (dot_index < file_name_index) {
        // No extension, but a dot is in a directory name.
        // Shouldn't happen because we search for the dot
        // starting at the file name.
        B_BUG();
        return end;
    } else {
        return dot;
    }
}

static char *
path_extensions_(
        B_FilePath *path) {
    return const_cast<char *>(path_extensions_(
        const_cast<char const *>(path)));
}

static B_FUNC
path_replacing_extensions_(
        B_FilePath const *path,
        char const *new_extensions,
        B_OUTPTR B_FilePath **out,
        B_ErrorHandler const *eh) {
    size_t new_extensions_length = strlen(new_extensions);
    char *new_path;
    if (!b_strdupplus(
            path, new_extensions_length, &new_path, eh)) {
        return false;
    }
    char *extensions = path_extensions_(new_path);
    memcpy(
        extensions,
        new_extensions,
        new_extensions_length + 1);
    *out = new_path;
    return true;
}

static B_FUNC
run_link(
        B_FilePath const *,
        B_AnswerContext const *answer_context,
        B_ProcessController *,
        B_ErrorHandler const *eh);

static B_FUNC
run_c_compile(
        B_FilePath const *,
        B_AnswerContext const *answer_context,
        B_ProcessController *,
        B_ErrorHandler const *eh);

static B_FUNC
run_cc_compile(
        B_FilePath const *,
        B_AnswerContext const *answer_context,
        B_ProcessController *,
        B_ErrorHandler const *eh);

static B_FUNC
check_file(
        B_FilePath const *,
        B_AnswerContext const *answer_context,
        B_ErrorHandler const *eh);

static B_FUNC
dispatch_question(
        B_AnswerContext const *answer_context,
        void *opaque,
        B_ErrorHandler const *eh) {
    auto process_controller
        = static_cast<B_ProcessController *>(opaque);

    if (answer_context->question_vtable->uuid
            != b_file_contents_question_vtable()->uuid) {
        (void) B_RAISE_ERRNO_ERROR(
            eh,
            EINVAL,
            "dispatch_question");
        return false;
    }

    auto path = static_cast<B_FilePath const *>(
        static_cast<void const *>(
            answer_context->question));
    char const *extension = path_extensions_(path);
    if (strcmp(extension, "") == 0) {
        return run_link(
            path,
            answer_context,
            process_controller,
            eh);
    } else if (strcmp(extension, ".c.o") == 0) {
        return run_c_compile(
            path,
            answer_context,
            process_controller,
            eh);
    } else if (strcmp(extension, ".cc.o") == 0) {
        return run_cc_compile(
            path,
            answer_context,
            process_controller,
            eh);
    } else {
        return check_file(path, answer_context, eh);
    }
}

static B_FUNC
run_link(
        B_FilePath const *exe_path,
        B_AnswerContext const *answer_context,
        B_ProcessController *process_controller,
        B_ErrorHandler const *eh) {
    static B_FilePath const *o_paths[] = {
        "ex/1/main.cc.o",
        "src/Alloc.c.o",
        "src/AnswerContext.c.o",
        "src/Assert.c.o",
        "src/Database.c.o",
        "src/Deserialize.c.o",
        "src/Error.c.o",
        "src/File.c.o",
        "src/FilePath.c.o",
        "src/Log.c.o",
        "src/Main.c.o",
        "src/Misc.c.o",
        "src/OS.c.o",
        "src/Process.c.o",
        "src/QuestionAnswer.c.o",
        "src/QuestionDispatch.c.o",
        "src/QuestionQueue.c.o",
        "src/QuestionQueueRoot.c.o",
        "src/QuestionVTableSet.c.o",
        "src/RefCount.c.o",
        "src/SQLite3.c.o",
        "src/Serialize.c.o",
        "src/Thread.c.o",
        "src/UUID.c.o",
        "vendor/sqlite-3.8.4.1/sqlite3.c.o",
    };
    size_t questions_count
        = sizeof(o_paths) / sizeof(*o_paths);
    auto questions
        = reinterpret_cast<B_Question const **>(o_paths);

    B_QuestionVTable const **questions_vtables;
    if (!b_allocate(
            questions_count * sizeof(*questions_vtables),
            reinterpret_cast<void **>(&questions_vtables),
            eh)) {
        return false;
    }
    for (size_t i = 0; i < questions_count; ++i) {
        questions_vtables[i]
            = b_file_contents_question_vtable();
    }

    bool ok = b_answer_context_need(
        answer_context,
        questions,
        questions_vtables,
        questions_count,
        [=](
                B_Answer *const *,
                B_ErrorHandler const *eh) {
            static char const *args_prefix[] = {
                "clang++",
                EXTRA_LDFLAGS
                "-Iinclude",
                "-o",
            };
            size_t args_prefix_count = sizeof(args_prefix)
                / sizeof(*args_prefix);

            size_t args_count
                = args_prefix_count + questions_count + 2;
            char const **args;
            if (!b_allocate(
                    args_count * sizeof(*args),
                    reinterpret_cast<void **>(&args),
                    eh)) {
                return false;
            }
            char const **cur_arg = args;
            for (size_t i = 0; i < args_prefix_count; ++i) {
                *cur_arg++ = args_prefix[i];
            }
            *cur_arg++ = exe_path;
            for (size_t i = 0; i < questions_count; ++i) {
                *cur_arg++ = o_paths[i];
            }
            *cur_arg++ = nullptr;

            return b_answer_context_exec(
                answer_context,
                process_controller,
                args,
                eh);
        },
        [](
                B_ErrorHandler const *) {
            // Do nothing.
            return true;
        },
        eh);

    (void) b_deallocate(questions_vtables, eh);
    return ok;
}

static B_FUNC
run_c_compile(
        B_FilePath const *o_path,
        B_AnswerContext const *answer_context,
        B_ProcessController *process_controller,
        B_ErrorHandler const *eh) {
    B_FilePath *c_path;
    if (!path_replacing_extensions_(
            o_path, ".c", &c_path, eh)) {
        return false;
    }
    // TODO(strager): Clean up c_path properly.

    auto question = static_cast<B_Question const *>(
        static_cast<void const *>(c_path));
    return b_answer_context_need_one(
        answer_context,
        question,
        b_file_contents_question_vtable(),
        [=](
                B_Answer *,
                B_ErrorHandler const *eh) {
            char const *command[] = {
                "clang",
                "-std=c99",
                "-Ivendor/sqlite-3.8.4.1",
                "-D_POSIX_SOURCE",
                "-D_POSIX_C_SOURCE=200112L",
                "-D_DARWIN_C_SOURCE",
                EXTRA_CFLAGS
                "-Iinclude",
                "-o", o_path,
                "-c",
                c_path,
                nullptr,
            };
            return b_answer_context_exec(
                answer_context,
                process_controller,
                command,
                eh);
        },
        [](
                B_ErrorHandler const *) {
            // Do nothing.
            return true;
        },
        eh);
}

static B_FUNC
run_cc_compile(
        B_FilePath const *o_path,
        B_AnswerContext const *answer_context,
        B_ProcessController *process_controller,
        B_ErrorHandler const *eh) {
    B_FilePath *cc_path;
    if (!path_replacing_extensions_(
            o_path, ".cc", &cc_path, eh)) {
        return false;
    }
    // TODO(strager): Clean up cc_path properly.

    // TODO(strager): Use std::make_unique.
    auto question = static_cast<B_Question const *>(
        static_cast<void const *>(cc_path));
    return b_answer_context_need_one(
        answer_context,
        question,
        b_file_contents_question_vtable(),
        [=](
                B_Answer *,
                B_ErrorHandler const *eh) {
            char const *command[] = {
                "clang++",
                "-std=c++11",
                "-Ivendor/sqlite-3.8.4.1",
                EXTRA_CXXFLAGS
                "-Iinclude",
                "-o", o_path,
                "-c",
                cc_path,
                nullptr,
            };
            return b_answer_context_exec(
                answer_context,
                process_controller,
                command,
                eh);
        },
        [](
                B_ErrorHandler const *) {
            // Do nothing.
            return true;
        },
        eh);
}

static B_FUNC
check_file(
        B_FilePath const *path,
        B_AnswerContext const *answer_context,
        B_ErrorHandler const *eh) {
    struct stat stat_buffer;
    int rc = stat(path, &stat_buffer);
    if (rc == -1) {
        switch (errno) {
        case ENOENT:
            return b_answer_context_error(
                    answer_context,
                    eh);

        default:
            // TODO(strager): Properly report errors.
            B_ErrorHandlerResult result
                    = B_RAISE_ERRNO_ERROR(
                        eh,
                        errno,
                        "stat");
            (void) result;  // TODO(strager)
            return false;
        }
    }

    return b_answer_context_success(answer_context, eh);
}

int
main(
        int,
        char **) {
    B_ErrorHandler const *eh = nullptr;

    // HACK(strager)
    auto initial_question = static_cast<B_Question *>(
        static_cast<void *>(const_cast<char *>("ex1")));

    B_QuestionVTableSet *question_vtable_set;
    if (!b_question_vtable_set_allocate(
            &question_vtable_set, eh)) {
        return 1;
    }
    if (!b_question_vtable_set_add(
            question_vtable_set,
            b_file_contents_question_vtable(),
            eh)) {
        return 1;
    }

    B_Database *database;
    if (!b_database_load_sqlite(
            "b_database.sqlite3",
            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
            nullptr,
            &database,
            eh)) {
        return 1;
    }
    if (!b_database_recheck_all(
            database, question_vtable_set, eh)) {
        return 1;
    }

    B_Main *main;
    if (!b_main_allocate(&main, eh)) {
        return 1;
    }

    B_ProcessController *process_controller;
    if (!b_main_process_controller(
            main, &process_controller, eh)) {
        return 1;
    }

    B_Answer *answer;
    if (!b_main_loop(
            main,
            initial_question,
            b_file_contents_question_vtable(),
            database,
            &answer,
            dispatch_question,
            process_controller,
            eh)) {
        return 1;
    }

    return answer ? 0 : 1;
}
