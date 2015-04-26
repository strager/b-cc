#pragma once

#include <B/Attributes.h>

#include <stdbool.h>
#include <stddef.h>

struct B_Error;

struct B_RunLoop;

typedef B_FUNC bool
B_RunLoopCallback(
    B_BORROW struct B_RunLoop *,
    B_BORROW void const *callback_data,
    B_OUT struct B_Error *);

#if defined(__cplusplus)
extern "C" {
#endif

B_WUR B_EXPORT_FUNC bool
b_run_loop_allocate(
    B_OUT_TRANSFER struct B_RunLoop **,
    B_OUT struct B_Error *);

B_WUR B_EXPORT_FUNC bool
b_run_loop_deallocate(
    B_TRANSFER struct B_RunLoop *,
    B_OUT struct B_Error *);

B_WUR B_EXPORT_FUNC bool
b_run_loop_add_callback(
    B_BORROW struct B_RunLoop *,
    B_TRANSFER B_RunLoopCallback *callback,
    B_TRANSFER B_RunLoopCallback *cancel_callback,
    B_TRANSFER void const *callback_data,
    size_t callback_data_size,
    B_OUT struct B_Error *);

B_WUR B_EXPORT_FUNC bool
b_run_loop_step(
    B_BORROW struct B_RunLoop *,
    B_OUT bool *keep_going,
    B_OUT struct B_Error *);

#if defined(__cplusplus)
}
#endif
