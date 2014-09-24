#include <B/Config.h>
#include <B/Error.h>
#include <B/Thread.h>

#if defined(B_CONFIG_PTHREAD)
# include <pthread.h>

B_EXPORT_FUNC
b_pthread_mutex_lock(
        pthread_mutex_t *mutex,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, mutex);

retry:;
    int rc = pthread_mutex_lock(mutex);
    if (rc != 0) {
        switch (B_RAISE_ERRNO_ERROR(
                eh, rc, "pthread_mutex_lock")) {
        case B_ERROR_ABORT:
        case B_ERROR_IGNORE:
            return false;
        case B_ERROR_RETRY:
            goto retry;
        }
    }
    return true;
}

B_EXPORT_FUNC
b_pthread_mutex_unlock(
        pthread_mutex_t *mutex,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, mutex);

retry:;
    int rc = pthread_mutex_unlock(mutex);
    if (rc != 0) {
        switch (B_RAISE_ERRNO_ERROR(
                eh, rc, "pthread_mutex_unlock")) {
        case B_ERROR_ABORT:
        case B_ERROR_IGNORE:
            return false;
        case B_ERROR_RETRY:
            goto retry;
        }
    }
    return true;
}

#endif
