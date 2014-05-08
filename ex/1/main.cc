#include <B/Alloc.h>
#include <B/AnswerContext.h>
#include <B/Assert.h>
#include <B/Database.h>
#include <B/DependencyDelegate.h>
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

// HACK HACK(strager)
#define BUILDING_ON_STRAGERS_MACHINE

// HACK(strager)
#if defined(__APPLE__)
# define EXTRA_CXXFLAGS "-stdlib=libc++",
# define EXTRA_LDFLAGS "-stdlib=libc++",
#elif defined(__linux__)
# define EXTRA_LDFLAGS "-lpthread",
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
        B_ProcessLoop *,
        B_ErrorHandler const *eh);

static B_FUNC
run_c_compile(
        std::string const &path,
        B_AnswerContext const *answer_context,
        B_ProcessLoop *,
        B_ErrorHandler const *eh);

static B_FUNC
run_cc_compile(
        std::string const &path,
        B_AnswerContext const *answer_context,
        B_ProcessLoop *,
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
    auto main_closure
        = static_cast<B_MainClosure *>(opaque);
    B_ProcessLoop *process_loop
        = main_closure->process_loop;

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
            process_loop,
            eh);
    } else if (extension == ".c.o") {
        return run_c_compile(
            path,
            answer_context,
            process_loop,
            eh);
    } else if (extension == ".cc.o") {
        return run_cc_compile(
            path,
            answer_context,
            process_loop,
            eh);
    } else {
        return check_file(path, answer_context, eh);
    }
}

static B_FUNC
run_link(
        std::string const &exe_path,
        B_AnswerContext const *answer_context,
        B_ProcessLoop *process_loop,
        B_ErrorHandler const *eh) {
    std::vector<std::string> o_paths = {
        "ex/1/main.cc.o",
        "src/Alloc.c.o",
        "src/AnswerContext.c.o",
        "src/Database.c.o",
        "src/Deserialize.c.o",
        "src/Error.c.o",
        "src/File.cc.o",
        "src/FilePath.c.o",
        "src/Log.c.o",
        "src/Main.c.o",
        "src/Misc.c.o",
        "src/Process.c.o",
        "src/QuestionAnswer.c.o",
        "src/QuestionDispatch.c.o",
        "src/QuestionQueue.cc.o",
        "src/QuestionVTableSet.c.o",
        "src/RefCount.c.o",
        "src/SQLite3.c.o",
        "src/Serialize.c.o",
        "src/Thread.c.o",
        "src/UUID.c.o",
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
        [answer_context, exe_path, o_paths, process_loop](
                B_Answer *const *,
                B_ErrorHandler const *eh) {
            std::vector<char const *> args = {
                "clang++",
#if defined(BUILDING_ON_STRAGERS_MACHINE)
                "-L/usr/local/opt/sqlite/lib",
#endif
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
            args.push_back("-lsqlite3");
            args.push_back(nullptr);
            return b_answer_context_exec(
                answer_context,
                process_loop,
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
        B_ProcessLoop *process_loop,
        B_ErrorHandler const *eh) {
    std::string c_path
            = path_drop_extensions(o_path) + ".c";

    auto question = static_cast<B_Question const *>(
        static_cast<void const *>(c_path.c_str()));
    return b_answer_context_need_one(
        answer_context,
        question,
        b_file_contents_question_vtable(),
        [answer_context, c_path, o_path, process_loop](
                B_Answer *,
                B_ErrorHandler const *eh) {
            char const *command[] = {
                "clang",
                "-std=c99",
#if defined(BUILDING_ON_STRAGERS_MACHINE)
                "-I/usr/local/opt/sqlite/include",
#endif
                EXTRA_CFLAGS
                "-Iinclude",
                "-o", o_path.c_str(),
                "-c",
                c_path.c_str(),
                nullptr,
            };
            return b_answer_context_exec(
                answer_context,
                process_loop,
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
        B_ProcessLoop *process_loop,
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
        [answer_context, cc_path, o_path, process_loop](
                B_Answer *,
                B_ErrorHandler const *eh) {
            char const *command[] = {
                "clang++",
                "-std=c++11",
#if defined(BUILDING_ON_STRAGERS_MACHINE)
                "-I/usr/local/opt/sqlite/include",
#endif
                EXTRA_CXXFLAGS
                "-Iinclude",
                "-o", o_path.c_str(),
                "-c",
                cc_path.c_str(),
                nullptr,
            };
            return b_answer_context_exec(
                answer_context,
                process_loop,
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

struct RootQueueItem_ :
        public B_QuestionQueueItemObject {
    template<typename TAnswerCallback>
    RootQueueItem_(
            B_Question *question,
            B_QuestionVTable const *question_vtable,
            TAnswerCallback answer_callback) :
            B_QuestionQueueItemObject {
                deallocate_,
                question,
                question_vtable,
                answer_callback_,
            },
            callback(answer_callback) {
    }

private:
    static B_FUNC
    deallocate_(
            B_QuestionQueueItemObject *queue_item,
            B_ErrorHandler const *eh) {
        return b_delete(
            static_cast<RootQueueItem_ *>(queue_item),
            eh);
    }

    static B_FUNC
    answer_callback_(
            B_TRANSFER B_OPT struct B_Answer *answer,
            void *opaque,
            struct B_ErrorHandler const *eh) {
        auto queue_item = static_cast<RootQueueItem_ *>(
            reinterpret_cast<B_QuestionQueueItemObject *>(
                opaque));
        return queue_item->callback(answer, opaque, eh);
    }

    std::function<B_AnswerCallback> callback;
};

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

    B_Answer *answer;
    if (!b_main(
            initial_question,
            b_file_contents_question_vtable(),
            &answer,
            "b_database.sqlite3",
            question_vtable_set.get(),
            dispatch_question,
            nullptr,
            eh)) {
        return 1;
    }

    return answer ? 0 : 1;
}
