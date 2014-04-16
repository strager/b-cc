#ifndef B_HEADER_GUARD_4F7E3BFB_AE92_40BA_BAA1_C7907F8737A5
#define B_HEADER_GUARD_4F7E3BFB_AE92_40BA_BAA1_C7907F8737A5

#include <B/Base.h>
#include <B/QuestionDispatch.h>

struct B_Answer;
struct B_ErrorHandler;
struct B_ProcessLoop;
struct B_Question;
struct B_QuestionVTable;
struct B_QuestionVTableSet;

struct B_MainClosure {
    struct B_ProcessLoop *process_loop;

    // Parameter to b_main.
    void *opaque;
};

#if defined(__cplusplus)
extern "C" {
#endif

// Runs the B dispatch loop, attempting to answer the given
// question.
//
// This is a convenience function which is merely a
// combination of the following function calls:
//
// 1. b_process_loop_allocate
// 2. b_process_loop_run_async_unsafe
// 3. b_question_queue_allocate
// 4. b_database_load_sqlite
// 5. b_database_recheck_all
// 6. b_question_queue_enqueue
// 7. b_question_dispatch
// 8-. Deallocation routines for steps 1-4.
//
// When dispatch_callback is called, the opaque parameter
// points to a MainClosure.
B_EXPORT_FUNC
b_main(
        struct B_Question const *initial_question,
        struct B_QuestionVTable const *initial_question_vtable,
        B_OUTPTR struct B_Answer **answer,
        char const *database_sqlite_path,
        struct B_QuestionVTableSet const *,
        B_QuestionDispatchCallback dispatch_callback,
        void *opaque,
        struct B_ErrorHandler const *);

#if defined(__cplusplus)
}
#endif

#endif