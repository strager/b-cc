#include <B/Config.h>
#include <B/Error.h>
#include <B/Assert.h>
#include <B/Private/Thread.h>

#if defined(B_CONFIG_PTHREAD)
# include <pthread.h>

B_FUNC
b_mutex_initialize(
        struct B_Mutex *mutex,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, mutex);
retry:;
    int rc = pthread_mutex_init(&mutex->mutex, NULL);
    if (rc != 0) {
        switch (B_RAISE_ERRNO_ERROR(
                eh, rc, "pthread_mutex_init")) {
            case B_ERROR_IGNORE:
            case B_ERROR_ABORT:
                return false;
            case B_ERROR_RETRY:
                goto retry;
        }
    }
    return true;
}

B_FUNC
b_mutex_destroy(
        struct B_Mutex *mutex,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, mutex);
retry:;
    int rc = pthread_mutex_destroy(&mutex->mutex);
    if (rc != 0) {
        switch (B_RAISE_ERRNO_ERROR(
                eh, rc, "pthread_mutex_init")) {
            case B_ERROR_IGNORE:
            case B_ERROR_ABORT:
                return false;
            case B_ERROR_RETRY:
                goto retry;
        }
    }
    return true;
}

void
b_mutex_lock(
        struct B_Mutex *mutex) {
    B_ASSERT(mutex);
    int rc = pthread_mutex_lock(&mutex->mutex);
    B_ASSERT(rc == 0);
}

void
b_mutex_unlock(
        struct B_Mutex *mutex) {
    B_ASSERT(mutex);
    int rc = pthread_mutex_unlock(&mutex->mutex);
    B_ASSERT(rc == 0);
}

#endif
