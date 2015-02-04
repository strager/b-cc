#include <B/Config.h>

// Decide the main loop implementation.
#if defined(B_CONFIG_KQUEUE)
// BSD kevent() with EVFILT_PROC (ProcessManager) and
// EVFILT_USER (QuestionQueue).
# define B_MAIN_KQUEUE_
#elif defined(B_CONFIG_EVENTFD) \
    && defined(B_CONFIG_POSIX_SIGNALS) \
    && !defined(B_CONFIG_BROKEN_PSELECT)
// POSIX pselect() with SIGCHLD unmasked (ProcessManager)
// and a Linux eventfd (QuestionQueue).
# define B_MAIN_EVENTFD_PSELECT_
#else
# error "Unknown implementation"
#endif

#include <B/Alloc.h>
#include <B/Assert.h>
#include <B/Database.h>
#include <B/Error.h>
#include <B/Log.h>
#include <B/Macro.h>
#include <B/Main.h>
#include <B/Process.h>
#include <B/QuestionAnswer.h>
#include <B/QuestionDispatch.h>
#include <B/QuestionQueue.h>

#if defined(B_MAIN_KQUEUE_) \
    || defined(B_MAIN_EVENTFD_PSELECT_)
# include <B/Private/OS.h>
#endif

#include <sqlite3.h>
#include <stdbool.h>
#include <stddef.h>

#if defined(B_MAIN_KQUEUE_)
# include <sys/event.h>
#elif defined(B_MAIN_EVENTFD_PSELECT_)
# include <errno.h>
# include <sys/select.h>
# include <unistd.h>
#endif

#if defined(B_MAIN_KQUEUE_)
enum {
    B_QUESTION_QUEUE_KQUEUE_USER_IDENT = 1,
};
#endif

struct ProcessControllerDelegate_ {
    struct B_ProcessControllerDelegate super;
#if defined(B_MAIN_KQUEUE_)
    int kqueue;  // fd
#endif
#if defined(B_MAIN_EVENTFD_PSELECT_)
    int question_queue_eventfd;
#endif
};

B_STATIC_ASSERT(
    offsetof(
        struct ProcessControllerDelegate_, super) == 0,
    "ProcessControllerDelegate_::super must be the first "
        "member");

struct B_Main {
    struct ProcessControllerDelegate_
        process_controller_delegate;
    struct B_ProcessManager *process_manager;
    struct B_Database *database;
    struct B_QuestionQueue *question_queue;
};

static B_FUNC
register_process_id_(
        struct B_ProcessControllerDelegate *,
        B_ProcessID,
        struct B_ErrorHandler const *);

static B_FUNC
unregister_process_id_(
        struct B_ProcessControllerDelegate *,
        B_ProcessID,
        struct B_ErrorHandler const *);

B_EXPORT_FUNC
b_main_allocate(
        B_OUTPTR struct B_Main **out_main,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, out_main);

    struct B_Main *main;
    if (!b_allocate(sizeof(*main), (void **) &main, eh)) {
        return false;
    }

    *main = (struct B_Main) {
        .process_controller_delegate = {
            .super = {
                .register_process_id = register_process_id_,
                .unregister_process_id
                    = unregister_process_id_,
                .register_process_handle = NULL,
                .unregister_process_handle = NULL,
            },
#if defined(B_MAIN_KQUEUE_)
            .kqueue = -1,
#elif defined(B_MAIN_EVENTFD_PSELECT_)
            .question_queue_eventfd = -1,
#endif
        },
        .process_manager = NULL,
        .question_queue = NULL,
    };
    struct ProcessControllerDelegate_ *pcd
        = &main->process_controller_delegate;

#if defined(B_MAIN_KQUEUE_)
    if (!b_kqueue(&pcd->kqueue, eh)) {
        goto fail;
    }
    // Create an auto-reset event and have the QuestionQueue
    // trigger it.
    if (!b_kevent(pcd->kqueue, &(struct kevent) {
                .ident = B_QUESTION_QUEUE_KQUEUE_USER_IDENT,
                .filter = EVFILT_USER,
                .flags = EV_ADD | EV_CLEAR,
                .fflags = NOTE_FFNOP,
                .data = (intptr_t) NULL,
                .udata = NULL,
            }, 1, NULL, 0, NULL, NULL, eh)) {
        goto fail;
    }
    if (!b_question_queue_allocate_with_kqueue(
            pcd->kqueue, &(struct kevent) {
                .ident = B_QUESTION_QUEUE_KQUEUE_USER_IDENT,
                .filter = EVFILT_USER,
                .flags = 0,
                .fflags = NOTE_TRIGGER,
                .data = (intptr_t) NULL,
                .udata = NULL,
            }, &main->question_queue, eh)) {
        goto fail;
    }
#elif defined(B_MAIN_EVENTFD_PSELECT_)
    if (!b_eventfd(
            0, 0, &pcd->question_queue_eventfd, eh)) {
        goto fail;
    }
    if (!b_question_queue_allocate_with_eventfd(
            pcd->question_queue_eventfd,
            &main->question_queue,
            eh)) {
        goto fail;
    }
#endif

    if (!b_process_manager_allocate(
            &pcd->super, &main->process_manager, eh)) {
        goto fail;
    }

    *out_main = main;
    return true;

fail:
    (void) b_main_deallocate(main, eh);
    return false;
}

B_EXPORT_FUNC
b_main_deallocate(
        B_TRANSFER struct B_Main *main,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, main);

    if (main->question_queue) {
        (void) b_question_queue_deallocate(
            main->question_queue, eh);
    }
    if (main->process_manager) {
        // FIXME(strager): Should we block waiting for
        // children to die or something?
        (void) b_process_manager_deallocate(
            main->process_manager, eh);
    }

    struct ProcessControllerDelegate_ *pcd
        = &main->process_controller_delegate;
#if defined(B_MAIN_KQUEUE_)
    if (pcd->kqueue != -1) {
        (void) b_close_fd(pcd->kqueue, eh);
    }
#elif defined(B_MAIN_EVENTFD_PSELECT_)
    if (pcd->question_queue_eventfd != -1) {
        (void) b_close_fd(pcd->question_queue_eventfd, eh);
    }
#endif

    (void) b_deallocate(main, eh);

    return true;
}

B_EXPORT_FUNC
b_main_process_controller(
        struct B_Main *main,
        B_OUT struct B_ProcessController **out,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, main);

    return b_process_manager_get_controller(
        main->process_manager, out, eh);
}

#if defined(B_MAIN_EVENTFD_PSELECT_)
static void
sigchld_handler_(
        int signal_number,
        siginfo_t *signal_info,
        void *thread_context) {
    // Do nothing.
    (void) signal_number;
    (void) signal_info;
    (void) thread_context;
}
#endif

B_EXPORT_FUNC
b_main_loop(
        struct B_Main *main,
        struct B_Question const *initial_question,
        struct B_QuestionVTable const *initial_question_vtable,
        struct B_Database *database,
        B_OUTPTR struct B_Answer **answer,
        B_QuestionDispatchCallback dispatch_callback,
        void *dispatch_callback_opaque,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, main);
    B_CHECK_PRECONDITION(eh, initial_question);
    B_CHECK_PRECONDITION(eh, initial_question_vtable);
    B_CHECK_PRECONDITION(eh, answer);
    B_CHECK_PRECONDITION(eh, dispatch_callback);

    bool ok;
    struct B_QuestionQueueRoot *question_queue_root = NULL;
    struct B_Answer *tmp_answer = NULL;

#if defined(B_MAIN_EVENTFD_PSELECT_)
    bool masked_sigchld = false;
    struct sigaction old_sigchld_sigaction;
    bool set_sigchld_sigaction = false;
#endif

    if (!b_question_queue_enqueue_root(
            main->question_queue,
            initial_question,
            initial_question_vtable,
            &question_queue_root,
            eh)) {
        goto fail;
    }

#if defined(B_MAIN_EVENTFD_PSELECT_)
    // Mask SIGCHLD so signals are queued until the next
    // pselect call.
    sigset_t mask;
    b_sigemptyset(&mask);
    b_sigaddset(&mask, SIGCHLD);
    if (!b_sigprocmask(SIG_BLOCK, &mask, NULL, eh)) {
        goto fail;
    }
    masked_sigchld = true;

    // The default signal handler (SIG_DFL) will cause
    // SIGCHLD to be discarded.  This means a SIGCHLD will
    // *not* interrupt (EINTR) pselect.  We must thus
    // install something if nothing has been installed
    // already.
    if (!b_sigaction(
            SIGCHLD, NULL, &old_sigchld_sigaction, eh)) {
        goto fail;
    }
    if (!(old_sigchld_sigaction.sa_flags & SA_SIGINFO)
            && (old_sigchld_sigaction.sa_handler
                == SIG_DFL)) {
        // There's a race condition here (if another thread
        // called sigaction), but mixing threads and signals
        // is unsafe anyway.
        // FIXME(strager): Provide a method (at least on
        // Linux) which avoids the problems of signals and
        // threads interacting.
        struct sigaction sa;
        b_sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_SIGINFO | SA_NOCLDSTOP;
        sa.sa_sigaction = sigchld_handler_;
        if (!b_sigaction(SIGCHLD, &sa, NULL, eh)) {
            goto fail;
        }
        set_sigchld_sigaction = true;
    }
#endif

    // Event loop.  Keep dispatching questions until the
    // QuestionQueue is closed.
    bool keep_going = true;
    while (keep_going) {
        bool received_process_event = false;
        bool received_queue_event = false;

#if defined(B_MAIN_KQUEUE_)
        int kqueue
            = main->process_controller_delegate.kqueue;
        size_t received_events;
        struct kevent events[10];
        if (!b_kevent(
                kqueue,
                NULL,
                0,
                events,
                sizeof(events) / sizeof(*events),
                NULL,
                &received_events,
                eh)) {
            goto fail;
        }
        for (size_t i = 0; i < received_events; ++i) {
            switch (events[i].filter) {
            case EVFILT_PROC:
                if (events[i].fflags & NOTE_EXIT) {
                    B_LOG(
                        B_DEBUG,
                        "Received NOTE_EXIT for process "
                            "%ld",
                        (long) events[i].ident);
                    received_process_event = true;
                }
                break;
            case EVFILT_USER:
                B_ASSERT(events[i].ident
                    == B_QUESTION_QUEUE_KQUEUE_USER_IDENT);
                B_LOG(
                    B_DEBUG,
                    "Received signal from QuestionQueue");
                received_queue_event = true;
                break;
            default:
                B_BUG();
                break;
            }
        }
#elif defined(B_MAIN_EVENTFD_PSELECT_)
        // Unblock SIGCHLD during pselect.  In other words,
        // allow pselect to return EINTR due to a SIGCHLD.
        sigset_t mask;
        if (!b_sigprocmask(0, NULL, &mask, eh)) {
            return false;
        }
        b_sigdelset(&mask, SIGCHLD);

        int eventfd = main->process_controller_delegate
            .question_queue_eventfd;
retry_pselect:;
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(eventfd, &rfds);
        int rc = pselect(
            eventfd + 1, &rfds, NULL, NULL, NULL, &mask);
        if (rc == 1) {
            B_ASSERT(FD_ISSET(eventfd, &rfds));
            B_LOG(
                B_DEBUG,
                "Received signal from QuestionQueue");
            received_queue_event = true;
            if (!b_eventfd_read(eventfd, NULL, eh)) {
                goto fail;
            }
        } else if (rc == -1) {
            if (errno == EINTR) {
                B_LOG(
                    B_DEBUG,
                    "Received EINTR (likely SIGCHLD)");
                received_process_event = true;
            } else {
                switch (B_RAISE_ERRNO_ERROR(
                        eh, errno, "pselect")) {
                case B_ERROR_ABORT:
                    goto fail;
                case B_ERROR_RETRY:
                    goto retry_pselect;
                case B_ERROR_IGNORE:
                    break;
                }
            }
        } else {
            B_BUG();
        }
#endif

        if (received_process_event) {
            if (!b_process_manager_check(
                    main->process_manager, eh)) {
                goto fail;
            }
        }
        if (received_queue_event) {
            // Keep dequeueing elements from the
            // QuestionQueue until the QuestionQueue is
            // empty.  We need to loop because a single
            // notification above may correspond to multiple
            // QuestionQueue signals.
            for (;;) {
                struct B_QuestionQueueItem *queue_item;
                bool question_queue_closed;
                if (!b_question_queue_try_dequeue(
                        main->question_queue,
                        &queue_item,
                        &question_queue_closed,
                        eh)) {
                    goto fail;
                }
                if (question_queue_closed) {
                    keep_going = false;
                    break;
                }
                if (!queue_item) {
                    break;
                }

                struct B_AnswerContext const *
                    answer_context;
                if (!b_question_dispatch_one(
                        queue_item,
                        main->question_queue,
                        database,
                        &answer_context,
                        eh)) {
                    goto fail;
                }
                if (answer_context) {
                    if (!dispatch_callback(
                            answer_context,
                            dispatch_callback_opaque,
                            eh)) {
                        goto fail;
                    }
                }
            }
        }
    }

    if (!b_question_queue_finalize_root(
            question_queue_root, &tmp_answer, eh)) {
        goto fail;
    }
    question_queue_root = NULL;  // Transferred.
    if (!tmp_answer) {
        (void) eh;  // TODO(strager)
        goto fail;
    }
    *answer = tmp_answer;

    ok = true;

done:
#if defined(B_MAIN_EVENTFD_PSELECT_)
    // Fix sigmask and sigaction ASAP.
    if (masked_sigchld) {
        sigset_t mask;
        b_sigemptyset(&mask);
        b_sigaddset(&mask, SIGCHLD);
        (void) b_sigprocmask(SIG_UNBLOCK, &mask, NULL, eh);
    }
    if (set_sigchld_sigaction) {
        (void) b_sigaction(
            SIGCHLD, &old_sigchld_sigaction, NULL, eh);
    }
#endif
    if (!ok && tmp_answer) {
        (void) initial_question_vtable->answer_vtable
            ->deallocate(tmp_answer, eh);
    }
    if (question_queue_root) {
        (void) b_question_queue_finalize_root(
            question_queue_root, &tmp_answer, eh);
        // FIXME(strager): We should deallocate tmp_answer
        // if necessary.
    }
    return ok;

fail:
    ok = false;
    goto done;
}

static B_FUNC
register_process_id_(
        struct B_ProcessControllerDelegate *raw_delegate,
        B_ProcessID process_id,
        struct B_ErrorHandler const *eh) {
    struct ProcessControllerDelegate_ *delegate
        = (void *) raw_delegate;
    B_CHECK_PRECONDITION(eh, delegate);
#if defined(B_MAIN_KQUEUE_)
    return b_kevent(delegate->kqueue, &(struct kevent) {
        .ident = process_id,
        .filter = EVFILT_PROC,
        .flags = EV_ADD,
        .fflags = NOTE_EXIT,
        .data = (intptr_t) NULL,
        .udata = delegate,
    }, 1, NULL, 0, NULL, NULL, eh);
#elif defined(B_MAIN_EVENTFD_PSELECT_)
    // Do nothing.
    (void) delegate;
    (void) process_id;
    return true;
#endif
}

static B_FUNC
unregister_process_id_(
        struct B_ProcessControllerDelegate *raw_delegate,
        B_ProcessID process_id,
        struct B_ErrorHandler const *eh) {
    struct ProcessControllerDelegate_ *delegate
        = (void *) raw_delegate;
    B_CHECK_PRECONDITION(eh, delegate);
#if defined(B_MAIN_KQUEUE_)
    return b_kevent(delegate->kqueue, &(struct kevent) {
        .ident = process_id,
        .filter = EVFILT_PROC,
        .flags = EV_DELETE,
        .fflags = NOTE_EXIT,
        .data = (intptr_t) NULL,
        .udata = delegate,
    }, 1, NULL, 0, NULL, NULL, eh);
#elif defined(B_MAIN_EVENTFD_PSELECT_)
    // Do nothing.
    (void) delegate;
    (void) process_id;
    return true;
#endif
}
