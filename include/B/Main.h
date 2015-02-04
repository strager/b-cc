#ifndef B_HEADER_GUARD_4F7E3BFB_AE92_40BA_BAA1_C7907F8737A5
#define B_HEADER_GUARD_4F7E3BFB_AE92_40BA_BAA1_C7907F8737A5

#include <B/Base.h>

struct B_Answer;
struct B_AnswerContext;
struct B_Database;
struct B_ErrorHandler;
struct B_ProcessController;
struct B_Question;
struct B_QuestionVTable;

// Main is a convenience class which sets up a
// ProcessController and a QuestionQueue.  Use Main as your
// program's entry point into b.
//
// Call b_main_loop to process ProcessController results and
// QuestionQueueItem-s.
//
// Thread-safe: NO
// Signal-safe: NO
// See also notes in b_main_loop.
struct B_Main;

typedef B_FUNC
B_QuestionDispatchCallback(
        struct B_AnswerContext const *,
        void *opaque,
        struct B_ErrorHandler const *);

#if defined(__cplusplus)
extern "C" {
#endif

B_EXPORT_FUNC
b_main_allocate(
        B_OUTPTR struct B_Main **,
        struct B_ErrorHandler const *);

B_EXPORT_FUNC
b_main_deallocate(
        B_TRANSFER struct B_Main *,
        struct B_ErrorHandler const *);

B_EXPORT_FUNC
b_main_process_controller(
        struct B_Main *,
        B_OUT struct B_ProcessController **,
        struct B_ErrorHandler const *);

// Runs the B dispatch loop, attempting to answer the given
// Question.  Answer-s and dependencies are recorded into
// the given Database.  Blocking.
//
// On some POSIX platforms (e.g. Linux), b_main_loop will
// block the SIGCHLD signal process-wide (using
// sigprocmask).  When this function returns, SIGCHLD is
// unblocked (or remains blocked if it was blocked before
// calling b_main).
B_EXPORT_FUNC
b_main_loop(
        struct B_Main *,
        struct B_Question const *initial_question,
        struct B_QuestionVTable const *initial_question_vtable,
        struct B_Database *database,
        B_OUTPTR struct B_Answer **answer,
        B_QuestionDispatchCallback dispatch_callback,
        void *dispatch_callback_opaque,
        struct B_ErrorHandler const *);

#if defined(__cplusplus)
}
#endif

#endif
