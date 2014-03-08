#include <B/AnswerContext.h>
#include <B/Assert.h>
#include <B/DependencyDelegate.h>
#include <B/Error.h>
#include <B/Process.h>
#include <B/QuestionAnswer.h>
#include <B/QuestionDispatch.h>
#include <B/QuestionQueue.h>

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <errno.h>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <vector>

// Initialized in main.
std::unique_ptr<B_ProcessLoop, B_ProcessLoopDeleter>
g_process_loop_;

struct FileQuestion :
        public B_Question {
    FileQuestion(
            std::string path) :
            path(path) {
    }

    std::string path;
};

bool
operator==(
        FileQuestion const &a,
        FileQuestion const &b) {
    return a.path == b.path;
}

struct FileAnswer : public B_Answer {
    // TODO(strager)
};

bool
operator==(
        FileAnswer const &,
        FileAnswer const &) {
    return true;  // TODO(strager)
}

static FileQuestion *
file_question(B_Question *q) {
    return static_cast<FileQuestion *>(q);
}

static FileQuestion const *
file_question(B_Question const *q) {
    return static_cast<FileQuestion const *>(q);
}

static FileAnswer const *
file_answer(B_Answer const *a) {
    return static_cast<FileAnswer const *>(a);
}

static B_AnswerVTable
file_answer_vtable = {
    // equal
    [](
            B_Answer const *answer_a,
            B_Answer const *answer_b,
            B_OUTPTR bool *out,
            B_ErrorHandler const *eh) {
        (void) eh;  // TODO(strager)
        FileAnswer const *a = file_answer(answer_a);
        FileAnswer const *b = file_answer(answer_b);
        *out = *a == *b;
        return true;
    },

    // replicate
    [](
            B_Answer const *,
            B_OUTPTR B_Answer **out,
            B_ErrorHandler const *eh) {
        (void) eh;  // TODO(strager)
        *out = new FileAnswer();  // TODO(strager)
        return true;
    },

    // deallocate
    [](
            B_Answer const *answer,
            B_ErrorHandler const *eh) {
        (void) eh;  // TODO(strager)
        delete file_answer(answer);
        return true;
    },
};

static B_QuestionVTable
file_question_vtable = {
    B_UUID_LITERAL("B6BD5D3B-DDC1-43B2-832B-2B5836BF78FC"),
    &file_answer_vtable,

    // begin_answer
    [](
            B_Question const *question,
            B_OUTPTR B_Answer **out,
            B_ErrorHandler const *eh) {
        B_CHECK_PRECONDITION(eh, out);

        // TODO(strager)
        (void) question;

        *out = new FileAnswer();
        return true;
    },

    // equal
    [](
            B_Question const *question_a,
            B_Question const *question_b,
            B_OUTPTR bool *out,
            B_ErrorHandler const *eh) {
        (void) eh;  // TODO(strager)
        FileQuestion const *a = file_question(question_a);
        FileQuestion const *b = file_question(question_b);
        *out = *a == *b;
        return true;
    },

    // replicate
    [](
            B_Question const *question,
            B_OUTPTR B_Question **out,
            B_ErrorHandler const *eh) {
        (void) eh;  // TODO(strager)
        FileQuestion const *fq = file_question(question);
        *out = new FileQuestion(*fq);
        return true;
    },

    // deallocate
    [](
            B_Question *question,
            B_ErrorHandler const *eh) {
        (void) eh;  // TODO(strager)
        delete file_question(question);
        return true;
    },
};

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
            != file_question_vtable.uuid) {
        (void) B_RAISE_ERRNO_ERROR(
            eh,
            EINVAL,
            "dispatch_question");
        return false;
    }

    FileQuestion const *fq
        = file_question(answer_context->question);
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
        "src/Error.c.o",
        "src/Error_cxx.cc.o",
        "src/Log.c.o",
        "src/Process-kqueue.c.o",
        "src/QuestionDispatch.c.o",
        "src/QuestionQueue.cc.o",
        "src/RefCount.c.o",
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
    std::vector<B_QuestionVTable *> questions_vtables_raw;
    std::fill_n(
            std::back_inserter(questions_vtables_raw),
            questions.size(),
            &file_question_vtable);
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
                "-stdlib=libc++",
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
        &file_question_vtable,
        [answer_context, c_path, o_path](
                B_Answer *answer,
                B_ErrorHandler const *eh) {
            // TODO(strager): Deallocate answer.
            (void) answer;

            char const *command[] = {
                "clang",
                "-std=c99",
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
        &file_question_vtable,
        [answer_context, cc_path, o_path](
                B_Answer *answer,
                B_ErrorHandler const *eh) {
            // TODO(strager): Deallocate answer.
            (void) answer;

            char const *command[] = {
                "clang++",
                "-std=c++11",
                "-stdlib=libc++",
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
            B_QuestionVTable *question_vtable,
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

    int exit_code;
    bool exit_code_set = false;

    auto initial_question = std::unique_ptr<FileQuestion>(
            new FileQuestion("ex1"));
    RootQueueItem_ *root_queue_item = new RootQueueItem_(
        initial_question.get(),
        &file_question_vtable,
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
                (void) file_question_vtable.answer_vtable
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

    B_DependencyDelegateObject dependency_delegate = {
        // dependency
        [](
                struct B_DependencyDelegateObject *,
                struct B_Question const *from,
                struct B_QuestionVTable const *from_vtable,
                struct B_Question const *to,
                struct B_QuestionVTable const *to_vtable,
                struct B_ErrorHandler const *eh) {
            // TODO(strager): Something interesting.
            (void) from;
            (void) from_vtable;
            (void) to;
            (void) to_vtable;
            (void) eh;
            return true;
        }
    };
    if (!b_question_dispatch(
            question_queue.get(),
            &dependency_delegate,
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
