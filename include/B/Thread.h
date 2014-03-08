#ifndef B_HEADER_GUARD_6D8AC133_7F4B_4AB2_890F_4EBC88728ABC
#define B_HEADER_GUARD_6D8AC133_7F4B_4AB2_890F_4EBC88728ABC

#include <B/Base.h>
#include <B/Config.h>

#if defined(B_CONFIG_PTHREAD)
# include <pthread.h>

struct B_ErrorHandler;

B_EXPORT_FUNC
b_pthread_mutex_lock(
        pthread_mutex_t *,
        struct B_ErrorHandler const *);

B_EXPORT_FUNC
b_pthread_mutex_unlock(
        pthread_mutex_t *,
        struct B_ErrorHandler const *);

#define B_MUTEX_LOCK(_lock, _eh) \
    (b_pthread_mutex_lock(&(_lock), (_eh)))

#define B_MUTEX_UNLOCK(_lock, _eh) \
    (b_pthread_mutex_unlock(&(_lock), (_eh)))

#define B_MUTEX_MUST_UNLOCK(_lock, _eh) \
    do { \
        if (!B_MUTEX_UNLOCK((_lock), (_eh))) { \
            B_ASSERT(0 && "Failed to unlock mutex"); \
        } \
    } while (0)

#else
# error "Unknown threads implementation"
#endif

#if defined(__cplusplus)
# include <B/Config.h>

# if defined(B_CONFIG_PTHREAD)
#  include <B/Assert.h>

# include <pthread.h>

// Thread-safe: NO
// Signal-safe: NO
class B_PthreadMutexHolder {
public:
    // Non-copyable.
    B_PthreadMutexHolder(
            B_PthreadMutexHolder const &) = delete;
    B_PthreadMutexHolder &
    operator=(
            B_PthreadMutexHolder const &) = delete;

    // Non-move-assignable.
    B_PthreadMutexHolder &
    operator=(
            B_PthreadMutexHolder &&) = delete;

    B_PthreadMutexHolder(
            pthread_mutex_t *mutex) :
            mutex(mutex) {
        B_ASSERT(mutex);

        int rc = pthread_mutex_lock(mutex);
        B_ASSERT(rc == 0);
    }

    B_PthreadMutexHolder(
            B_PthreadMutexHolder &&other) :
            B_PthreadMutexHolder(other.release()) {
    }

    ~B_PthreadMutexHolder() {
        if (mutex) {
            int rc = pthread_mutex_unlock(mutex);
            B_ASSERT(rc == 0);
        }
    }

    pthread_mutex_t *
    release() {
        pthread_mutex_t *mutex = this->mutex;
        this->mutex = nullptr;
        return mutex;
    }

private:
    pthread_mutex_t *mutex;
};

# endif
#endif

#endif
