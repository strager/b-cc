#pragma once

#include <B/Attributes.h>

#include <stdbool.h>
#include <stddef.h>

struct B_Error;

struct B_RunLoop;
struct B_RunLoopVTable;

typedef B_FUNC bool
B_RunLoopFunction(
    B_BORROW struct B_RunLoop *,
    B_BORROW void const *callback_data,
    B_OUT struct B_Error *);

struct B_RunLoopVTable {
  B_WUR B_FUNC void (*deallocate)(
      B_TRANSFER struct B_RunLoop *);

  B_WUR B_FUNC bool (*add_function)(
      B_BORROW struct B_RunLoop *,
      B_RunLoopFunction *callback,
      B_RunLoopFunction *cancel,
      B_BORROW void const *callback_data,
      size_t callback_data_size,
      B_BORROW struct B_Error *);

  B_WUR B_FUNC bool (*run)(
      B_BORROW struct B_RunLoop *,
      B_BORROW struct B_Error *);

  B_WUR B_FUNC bool (*stop)(
      B_BORROW struct B_RunLoop *,
      B_BORROW struct B_Error *);
};

struct B_RunLoop {
  struct B_RunLoopVTable vtable;
  // Abstract.
};

#if defined(__cplusplus)
extern "C" {
#endif

B_WUR B_EXPORT_FUNC bool
b_run_loop_allocate_preferred(
    B_OUT_TRANSFER struct B_RunLoop **,
    B_OUT struct B_Error *);

B_WUR B_EXPORT_FUNC bool
b_run_loop_allocate_plain(
    B_OUT_TRANSFER struct B_RunLoop **,
    B_OUT struct B_Error *);

B_WUR B_EXPORT_FUNC void
b_run_loop_deallocate(
    B_TRANSFER struct B_RunLoop *);

B_WUR B_EXPORT_FUNC bool
b_run_loop_add_function(
    B_BORROW struct B_RunLoop *,
    B_RunLoopFunction *callback,
    B_RunLoopFunction *cancel,
    B_BORROW void const *callback_data,
    size_t callback_data_size,
    B_OUT struct B_Error *);

B_WUR B_EXPORT_FUNC bool
b_run_loop_run(
    B_BORROW struct B_RunLoop *,
    B_OUT struct B_Error *);

B_WUR B_EXPORT_FUNC bool
b_run_loop_stop(
    B_BORROW struct B_RunLoop *,
    B_OUT struct B_Error *);

#if defined(__cplusplus)
}
#endif
