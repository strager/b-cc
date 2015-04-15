#include <B/Error.h>
#include <B/Private/Assertions.h>
#include <B/Private/Mutex.h>

#include <pthread.h>

B_WUR B_EXPORT_FUNC bool
b_mutex_initialize(
    B_OUT_TRANSFER struct B_Mutex *mutex,
    B_OUT struct B_Error *e) {
  B_OUT_PARAMETER(mutex);
  B_OUT_PARAMETER(e);

  int rc;
  pthread_mutexattr_t attr;
  rc = pthread_mutexattr_init(&attr);
  if (rc != 0) {
    *e = (struct B_Error) {.posix_error = rc};
    return false;
  }
  rc = pthread_mutexattr_settype(
    &attr, PTHREAD_MUTEX_ERRORCHECK);
  if (rc != 0) {
    (void) pthread_mutexattr_destroy(&attr);
    *e = (struct B_Error) {.posix_error = rc};
    return false;
  }
  rc = pthread_mutex_init(&mutex->mutex, &attr);
  if (rc != 0) {
    (void) pthread_mutexattr_destroy(&attr);
    *e = (struct B_Error) {.posix_error = rc};
    return false;
  }
  (void) pthread_mutexattr_destroy(&attr);
  return true;
}

B_WUR B_EXPORT_FUNC bool
b_mutex_destroy(
    B_TRANSFER struct B_Mutex *mutex,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(mutex);
  B_OUT_PARAMETER(e);

  int rc = pthread_mutex_destroy(&mutex->mutex);
  if (rc != 0) {
    *e = (struct B_Error) {.posix_error = rc};
    return false;
  }
  return true;
}

B_EXPORT_FUNC void
b_mutex_lock(
    B_BORROW struct B_Mutex *mutex) {
  B_PRECONDITION(mutex);

  int rc = pthread_mutex_lock(&mutex->mutex);
  B_ASSERT(rc == 0);
}

B_EXPORT_FUNC void
b_mutex_unlock(
    B_BORROW struct B_Mutex *mutex) {
  B_PRECONDITION(mutex);

  int rc = pthread_mutex_unlock(&mutex->mutex);
  B_ASSERT(rc == 0);
}
