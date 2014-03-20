// GCC 4.8.2 has a false warning.  With -Werror, this fails
// the build.  The following block changes the failure into
// a mere warning.
#if defined(__GNUC__) && !defined(__clang__)
# pragma GCC diagnostic warning "-Wmaybe-uninitialized"
#endif

#include <B/Alloc.h>
#include <B/AnswerContext.h>
#include <B/Assert.h>
#include <B/Database.h>
#include <B/DependencyDelegate.h>
#include <B/Error.h>
#include <B/Process.h>
#include <B/QuestionAnswer.h>
#include <B/QuestionDispatch.h>
#include <B/QuestionQueue.h>
#include <B/QuestionVTableSet.h>

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <errno.h>
#include <fstream>
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

// Initialized in main.
std::unique_ptr<B_ProcessLoop, B_ProcessLoopDeleter>
g_process_loop_(nullptr, nullptr);

struct FileAnswer :
        public B_AnswerClass<FileAnswer> {
    explicit
    FileAnswer(
            uint64_t sum_hash) :
            sum_hash(sum_hash) {
    }

    static uint64_t
    sum_hash_from_path(
            std::string path) {
        std::ifstream input(path);
        if (!input) {
            return 0;  // FIXME(strager)
        }
        uint64_t sum_hash = 0;
        char c;
        while (input.get(c)) {
            sum_hash += static_cast<uint8_t>(c);
        }
        return sum_hash;
    }

    static B_FUNC
    equal(
            FileAnswer const &a,
            FileAnswer const &b,
            B_OUTPTR bool *out,
            B_ErrorHandler const *eh) {
        (void) eh;  // TODO(strager)
        *out = a == b;
        return true;
    }

    B_FUNC
    replicate(
            B_OUTPTR FileAnswer **out,
            B_ErrorHandler const *eh) const {
        (void) eh;  // TODO(strager)
        *out = new FileAnswer(*this);
        return true;
    }

    B_FUNC
    deallocate(
            B_ErrorHandler const *eh) {
        (void) eh;  // TODO(strager)
        delete this;
        return true;
    }

    B_FUNC
    serialize(
            B_OUT B_Serialized *out,
            B_ErrorHandler const *eh) const {
        B_CHECK_PRECONDITION(eh, out);

        size_t size = sizeof(uint64_t);
        if (!b_allocate(size, &out->data, eh)) {
            return false;
        }
        // FIXME(strager): This is endianness-dependent.
        *reinterpret_cast<uint64_t *>(out->data)
            = this->sum_hash;
        out->size = size;
        return true;
    }

    static B_FUNC
    deserialize(
            B_BORROWED B_Serialized serialized,
            B_OUTPTR FileAnswer **out,
            B_ErrorHandler const *eh) {
        B_CHECK_PRECONDITION(eh, out);
        B_CHECK_PRECONDITION(eh, serialized.data);

        if (serialized.size != sizeof(uint64_t)) {
            // TODO(strager): Report error.
            return false;
        }
        // FIXME(strager): This is endianness-dependent.
        *out = new FileAnswer(*reinterpret_cast<uint64_t *>(
            serialized.data));
        return true;
    }

    bool
    operator==(
            FileAnswer const &other) const {
        return other.sum_hash == this->sum_hash;
    }

    // TODO(strager): Store a hash instead (e.g. SHA-256).
    uint64_t const sum_hash;
};

struct FileQuestion :
        public B_QuestionClass<FileQuestion> {
    typedef FileAnswer AnswerClass;

    explicit
    FileQuestion(
            std::string path) :
            path(path) {
    }

    std::string const path;

    B_FUNC
    answer(
            B_OUTPTR FileAnswer **out,
            B_ErrorHandler const *eh) const {
        B_CHECK_PRECONDITION(eh, out);

        uint64_t sum_hash
            = FileAnswer::sum_hash_from_path(this->path);
        *out = new FileAnswer(sum_hash);
        return true;
    }

    static B_FUNC
    equal(
            FileQuestion const &a,
            FileQuestion const &b,
            B_OUTPTR bool *out,
            B_ErrorHandler const *eh) {
        (void) eh;  // TODO(strager)
        *out = a == b;
        return true;
    }

    B_FUNC
    replicate(
            B_OUTPTR FileQuestion **out,
            B_ErrorHandler const *eh) const {
        (void) eh;  // TODO(strager)
        *out = new FileQuestion(*this);
        return true;
    }

    B_FUNC
    deallocate(
            B_ErrorHandler const *eh) {
        (void) eh;  // TODO(strager)
        delete this;
        return true;
    }

    B_FUNC
    serialize(
            B_OUT B_Serialized *out,
            B_ErrorHandler const *eh) const {
        size_t size = this->path.size();
        if (!b_memdup(
                this->path.c_str(),
                size,
                &out->data,
                eh)) {
            return false;
        }
        out->size = size;
        return true;
    }

    static B_FUNC
    deserialize(
            B_BORROWED B_Serialized serialized,
            B_OUTPTR FileQuestion **out,
            B_ErrorHandler const *eh) {
        B_CHECK_PRECONDITION(eh, out);

        *out = new FileQuestion(std::string(
            static_cast<char const *>(serialized.data),
            serialized.size));
        return true;
    }

    bool
    operator==(
            FileQuestion const &other) const {
        return other.path == this->path;
    }
};

template<>
B_UUID
B_QuestionClass<FileQuestion>::uuid
    = B_UUID_LITERAL("B6BD5D3B-DDC1-43B2-832B-2B5836BF78FC");

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
        FileQuestion const *file_question,
        B_AnswerContext const *answer_context,
        B_ErrorHandler const *eh);

static B_FUNC
run_c_compile(
        FileQuestion const *file_question,
        B_AnswerContext const *answer_context,
        B_ErrorHandler const *eh);

static B_FUNC
run_cc_compile(
        FileQuestion const *file_question,
        B_AnswerContext const *answer_context,
        B_ErrorHandler const *eh);

static B_FUNC
check_file(
        FileQuestion const *file_question,
        B_AnswerContext const *answer_context,
        B_ErrorHandler const *eh);

static B_FUNC
dispatch_question(
        B_AnswerContext const *answer_context,
        void *,
        B_ErrorHandler const *eh) {
    if (answer_context->question_vtable->uuid
            != FileQuestion::vtable()->uuid) {
        (void) B_RAISE_ERRNO_ERROR(
            eh,
            EINVAL,
            "dispatch_question");
        return false;
    }

    auto fq = static_cast<FileQuestion const *>(
        answer_context->question);
    std::string extension = path_extensions(fq->path);
    if (extension == "") {
        return run_link(fq, answer_context, eh);
    } else if (extension == ".c.o") {
        return run_c_compile(fq, answer_context, eh);
    } else if (extension == ".cc.o") {
        return run_cc_compile(fq, answer_context, eh);
    } else {
        return check_file(fq, answer_context, eh);
    }
}

static B_FUNC
run_link(
        FileQuestion const *file_question,
        B_AnswerContext const *answer_context,
        B_ErrorHandler const *eh) {
    std::string exe_path = file_question->path;

    // FIXME(strager): This is super ugly.
    // TODO(strager): Use std::make_unique.
    std::vector<std::string> o_paths = {
        "ex/1/main.cc.o",
        "src/Alloc.c.o",
        "src/AnswerContext.c.o",
        "src/Database.c.o",
        "src/Error.c.o",
        "src/Log.c.o",
        "src/Misc.c.o",
        "src/Process.c.o",
        "src/QuestionDispatch.c.o",
        "src/QuestionQueue.cc.o",
        "src/QuestionVTableSet.c.o",
        "src/RefCount.c.o",
        "src/SQLite3.c.o",
        "src/Thread.c.o",
        "src/UUID.c.o",
    };
    std::vector<std::unique_ptr<FileQuestion>> questions;
    std::transform(
        o_paths.begin(),
        o_paths.end(),
        std::back_inserter(questions),
        [](
                const std::string &o_path) {
            return std::unique_ptr<FileQuestion>(
                    new FileQuestion(o_path));
        });
    std::vector<B_Question const *> questions_raw;
    std::transform(
        questions.begin(),
        questions.end(),
        std::back_inserter(questions_raw),
        [](
                const std::unique_ptr<FileQuestion> &q) {
            return q.get();
        });
    std::vector<B_QuestionVTable const *>
        questions_vtables_raw;
    std::fill_n(
            std::back_inserter(questions_vtables_raw),
            questions.size(),
            FileQuestion::vtable());
    return b_answer_context_need(
        answer_context,
        questions_raw.data(),
        questions_vtables_raw.data(),
        questions_raw.size(),
        [answer_context, exe_path, o_paths](
                B_Answer *const *answers,
                B_ErrorHandler const *eh) {
            // TODO(strager): Deallocate answers.
            (void) answers;

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
                g_process_loop_.get(),
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
        FileQuestion const *file_question,
        B_AnswerContext const *answer_context,
        B_ErrorHandler const *eh) {
    std::string o_path = file_question->path;
    std::string c_path
            = path_drop_extensions(o_path) + ".c";

    // TODO(strager): Use std::make_unique.
    auto question = std::unique_ptr<FileQuestion>(
            new FileQuestion(c_path));
    return b_answer_context_need_one(
        answer_context,
        question.get(),
        FileQuestion::vtable(),
        [answer_context, c_path, o_path](
                B_Answer *answer,
                B_ErrorHandler const *eh) {
            // TODO(strager): Deallocate answer.
            (void) answer;

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
                g_process_loop_.get(),
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
        FileQuestion const *file_question,
        B_AnswerContext const *answer_context,
        B_ErrorHandler const *eh) {
    std::string o_path = file_question->path;
    std::string cc_path
            = path_drop_extensions(o_path) + ".cc";

    // TODO(strager): Use std::make_unique.
    auto question = std::unique_ptr<FileQuestion>(
            new FileQuestion(cc_path));
    return b_answer_context_need_one(
        answer_context,
        question.get(),
        FileQuestion::vtable(),
        [answer_context, cc_path, o_path](
                B_Answer *answer,
                B_ErrorHandler const *eh) {
            // TODO(strager): Deallocate answer.
            (void) answer;

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
                g_process_loop_.get(),
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
        FileQuestion const *file_question,
        B_AnswerContext const *answer_context,
        B_ErrorHandler const *eh) {
    struct stat stat_buffer;
    int rc = stat(
            file_question->path.c_str(),
            &stat_buffer);
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

    B_ProcessLoop *process_loop_raw;
    if (!b_process_loop_allocate(
            1,
            b_process_auto_configuration_unsafe(),
            &process_loop_raw,
            eh)) {
        return 1;
    }
    g_process_loop_.reset(process_loop_raw);
    process_loop_raw = nullptr;

    if (!b_process_loop_run_async_unsafe(
            g_process_loop_.get(),
            eh)) {
        return 1;
    }

    B_QuestionQueue *question_queue_raw;
    if (!b_question_queue_allocate(
            &question_queue_raw,
            eh)) {
        return 1;
    }
    std::unique_ptr<B_QuestionQueue, B_QuestionQueueDeleter>
        question_queue(question_queue_raw, eh);
    question_queue_raw = nullptr;

    B_Database *database_raw;
    if (!b_database_load_sqlite(
            "b_database.sqlite3",
            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
            NULL,
            &database_raw,
            eh)) {
        return 1;
    }
    std::unique_ptr<B_Database, B_DatabaseDeleter>
        database(database_raw, eh);
    database_raw = nullptr;

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
            FileQuestion::vtable(),
            eh)) {
        return 1;
    }

    if (!b_database_recheck_all(
            database.get(),
            question_vtable_set.get(),
            eh)) {
        return 1;
    }

    int exit_code;
    bool exit_code_set = false;

    auto initial_question = std::unique_ptr<FileQuestion>(
            new FileQuestion("ex1"));
    RootQueueItem_ *root_queue_item = new RootQueueItem_(
        initial_question.get(),
        FileQuestion::vtable(),
        [&question_queue, &exit_code, &exit_code_set](
                B_TRANSFER B_OPT B_Answer *answer,
                void *,
                B_ErrorHandler const *eh) {
            if (!b_question_queue_close(
                    question_queue.get(),
                    eh)) {
                return false;
            }

            if (answer) {
                (void) FileQuestion::vtable()->answer_vtable
                        ->deallocate(answer, eh);
            }

            exit_code = answer == NULL ? 1 : 0;
            exit_code_set = true;
            return true;
        });

    if (!b_question_queue_enqueue(
            question_queue.get(),
            root_queue_item,
            eh)) {
        return 1;
    }

    if (!b_question_dispatch(
            question_queue.get(),
            database.get(),
            dispatch_question,
            nullptr,
            eh)) {
        return 1;
    }

    if (!exit_code_set) {
        fprintf(stderr, "b: FATAL: Exit code not set\n");
        return 1;
    }
    return exit_code;
}
