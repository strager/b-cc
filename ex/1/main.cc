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
#include <errno.h>
#include <memory>
#include <sqlite3.h>
#include <string>
#include <sys/stat.h>
#include <vector>

// HACK(strager)
#if defined(__APPLE__)
# define EXTRA_CXXFLAGS "-stdlib=libc++",
# define EXTRA_LDFLAGS "-stdlib=libc++",
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

static std::string::size_type
path_extensions_index(
        std::string path) {
    // TODO(strager): Support non-UNIX path separators.
    std::string::size_type slash_index = path.rfind('/');
    std::string::size_type file_name_index
            = slash_index == std::string::npos
            ? 0
            : slash_index + 1;
    std::string::size_type dot_index
            = path.find('.', file_name_index);
    if (dot_index == std::string::npos) {
        // No extension.
        return path.size();
    } else if (dot_index == file_name_index) {
        // Dot file.
        return path.size();
    } else if (dot_index < file_name_index) {
        // No extension, but a dot is in a directory name.
        // Shouldn't happen because we search for the dot
        // starting at the file name.
        B_BUG();
        return path.size();
    } else {
        return dot_index;
    }
}

static std::string
path_extensions(
        std::string path) {
    return std::string(
            path,
            path_extensions_index(path),
            std::string::npos);
}

static std::string
path_drop_extensions(
        std::string path) {
    return std::string(
            path,
            0,
            path_extensions_index(path));
}

static B_FUNC
run_link(
        std::string const &path,
        B_AnswerContext const *answer_context,
        B_ProcessController *,
        B_ErrorHandler const *eh);

static B_FUNC
run_c_compile(
        std::string const &path,
        B_AnswerContext const *answer_context,
        B_ProcessController *,
        B_ErrorHandler const *eh);

static B_FUNC
run_cc_compile(
        std::string const &path,
        B_AnswerContext const *answer_context,
        B_ProcessController *,
        B_ErrorHandler const *eh);

static B_FUNC
check_file(
        std::string const &path,
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

    auto path_raw = static_cast<char const *>(
        static_cast<void const *>(
            answer_context->question));
    std::string path(path_raw);
    std::string extension = path_extensions(path);
    if (extension == "") {
        return run_link(
            path,
            answer_context,
            process_controller,
            eh);
    } else if (extension == ".c.o") {
        return run_c_compile(
            path,
            answer_context,
            process_controller,
            eh);
    } else if (extension == ".cc.o") {
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
        std::string const &exe_path,
        B_AnswerContext const *answer_context,
        B_ProcessController *process_controller,
        B_ErrorHandler const *eh) {
    std::vector<std::string> o_paths = {
        "ex/1/main.cc.o",
        "src/Alloc.c.o",
        "src/AnswerContext.c.o",
        "src/Assert.c.o",
        "src/Database.c.o",
        "src/Deserialize.c.o",
        "src/Error.c.o",
        "src/File.cc.o",
        "src/FilePath.c.o",
        "src/Log.c.o",
        "src/Main.c.o",
        "src/Misc.c.o",
        "src/OS.c.o",
        "src/Process.c.o",
        "src/QuestionAnswer.c.o",
        "src/QuestionDispatch.c.o",
        "src/QuestionQueue.cc.o",
        "src/QuestionQueueRoot.c.o",
        "src/QuestionVTableSet.c.o",
        "src/RefCount.c.o",
        "src/SQLite3.c.o",
        "src/Serialize.c.o",
        "src/Thread.c.o",
        "src/UUID.c.o",
        "vendor/sqlite-3.8.4.1/sqlite3.c.o",
    };
    std::vector<B_Question const *> questions;
    std::transform(
        o_paths.begin(),
        o_paths.end(),
        std::back_inserter(questions),
        [](
                const std::string &path) {
            return static_cast<B_Question const *>(
                static_cast<void const *>(
                    path.c_str()));
        });
    std::vector<B_QuestionVTable const *>
        questions_vtables;
    std::fill_n(
            std::back_inserter(questions_vtables),
            questions.size(),
            b_file_contents_question_vtable());
    return b_answer_context_need(
        answer_context,
        questions.data(),
        questions_vtables.data(),
        questions.size(),
        [=](
                B_Answer *const *,
                B_ErrorHandler const *eh) {
            std::vector<char const *> args = {
                "clang++",
                EXTRA_LDFLAGS
                "-Iinclude",
                "-o", exe_path.c_str(),
            };
            std::transform(
                o_paths.begin(),
                o_paths.end(),
                std::back_inserter(args),
                [](
                        const std::string &o_path) {
                    return o_path.c_str();
                });
            args.push_back(nullptr);
            return b_answer_context_exec(
                answer_context,
                process_controller,
                args.data(),
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
run_c_compile(
        std::string const &o_path,
        B_AnswerContext const *answer_context,
        B_ProcessController *process_controller,
        B_ErrorHandler const *eh) {
    std::string c_path
            = path_drop_extensions(o_path) + ".c";

    auto question = static_cast<B_Question const *>(
        static_cast<void const *>(c_path.c_str()));
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
                "-o", o_path.c_str(),
                "-c",
                c_path.c_str(),
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
        std::string const &o_path,
        B_AnswerContext const *answer_context,
        B_ProcessController *process_controller,
        B_ErrorHandler const *eh) {
    std::string cc_path
            = path_drop_extensions(o_path) + ".cc";

    // TODO(strager): Use std::make_unique.
    auto question = static_cast<B_Question const *>(
        static_cast<void const *>(cc_path.c_str()));
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
                "-o", o_path.c_str(),
                "-c",
                cc_path.c_str(),
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
        std::string const &path,
        B_AnswerContext const *answer_context,
        B_ErrorHandler const *eh) {
    struct stat stat_buffer;
    int rc = stat(path.c_str(), &stat_buffer);
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

    B_QuestionVTableSet *question_vtable_set_raw;
    if (!b_question_vtable_set_allocate(
            &question_vtable_set_raw,
            eh)) {
        return 1;
    }
    std::unique_ptr<
            B_QuestionVTableSet,
            B_QuestionVTableSetDeleter>
        question_vtable_set(question_vtable_set_raw, eh);
    question_vtable_set_raw = nullptr;
    if (!b_question_vtable_set_add(
            question_vtable_set.get(),
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
            database, question_vtable_set.get(), eh)) {
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
