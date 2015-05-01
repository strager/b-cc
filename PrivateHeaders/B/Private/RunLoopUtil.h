#pragma once

#include <B/Attributes.h>
#include <B/Private/Config.h>
#include <B/Private/Queue.h>
#include <B/RunLoop.h>

#include <stdbool.h>
#include <stddef.h>

struct B_Error;
struct B_ProcessExitStatus;

struct B_RunLoopFunctionEntry;

typedef B_SLIST_HEAD(, B_RunLoopFunctionEntry)
B_RunLoopFunctionList;

#if defined(__cplusplus)
extern "C" {
#endif

B_FUNC void
b_run_loop_function_list_initialize(
    B_OUT_TRANSFER B_RunLoopFunctionList *);

B_FUNC void
b_run_loop_function_list_deinitialize(
    B_BORROW struct B_RunLoop *,
    B_TRANSFER B_RunLoopFunctionList *);

B_WUR B_FUNC bool
b_run_loop_function_list_add_function(
    B_BORROW B_RunLoopFunctionList *,
    B_TRANSFER B_RunLoopFunction *callback,
    B_TRANSFER B_RunLoopFunction *cancel_callback,
    B_TRANSFER void const *callback_data,
    size_t callback_data_size,
    B_OUT struct B_Error *);

B_FUNC void
b_run_loop_function_list_run_one(
    B_BORROW struct B_RunLoop *,
    B_BORROW B_RunLoopFunctionList *,
    B_OUT bool *keep_going);

#if B_CONFIG_POSIX_PROCESS
B_WUR B_FUNC struct B_ProcessExitStatus
b_exit_status_from_waitpid_status(
    int waitpid_status);
#endif

#if defined(__cplusplus)
}
#endif
