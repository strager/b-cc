#pragma once

#include <B/Attributes.h>

#include <pthread.h>
#include <stdbool.h>

struct B_Error;

struct B_Mutex {
  pthread_mutex_t mutex;
};

#if defined(__cplusplus)
extern "C" {
#endif

B_WUR B_EXPORT_FUNC bool
b_mutex_initialize(
    B_OUT_TRANSFER struct B_Mutex *,
    B_OUT struct B_Error *);

B_WUR B_EXPORT_FUNC bool
b_mutex_destroy(
    B_TRANSFER struct B_Mutex *,
    B_OUT struct B_Error *);

B_EXPORT_FUNC void
b_mutex_lock(
    B_BORROW struct B_Mutex *);

B_EXPORT_FUNC void
b_mutex_unlock(
    B_BORROW struct B_Mutex *);

#if defined(__cplusplus)
}
#endif
