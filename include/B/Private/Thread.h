#ifndef B_HEADER_GUARD_6D8AC133_7F4B_4AB2_890F_4EBC88728ABC
#define B_HEADER_GUARD_6D8AC133_7F4B_4AB2_890F_4EBC88728ABC

#include <B/Base.h>
#include <B/Config.h>

#if defined(B_CONFIG_PTHREAD)
# include <pthread.h>
#endif

struct B_ErrorHandler;

struct B_Mutex {
#if defined(B_CONFIG_PTHREAD)
    pthread_mutex_t mutex;
#else
# error "Unknown threads implementation"
#endif
};

#if defined(__cplusplus)
extern "C" {
#endif

B_FUNC
b_mutex_initialize(
        struct B_Mutex *,
        struct B_ErrorHandler const *);

B_FUNC
b_mutex_destroy(
        struct B_Mutex *,
        struct B_ErrorHandler const *);

void
b_mutex_lock(
        struct B_Mutex *);

void
b_mutex_unlock(
        struct B_Mutex *);

#if defined(__cplusplus)
}
#endif

#if defined(__cplusplus)
# include <B/Config.h>

// Thread-safe: NO
// Signal-safe: NO
class B_MutexHolder {
public:
    // Non-copyable.
    B_MutexHolder(
            B_MutexHolder const &) = delete;
    B_MutexHolder &
    operator=(B_MutexHolder const &) = delete;

    // Non-move-assignable.
    B_MutexHolder &
    operator=(B_MutexHolder &&) = delete;

    B_MutexHolder(B_Mutex *mutex) :
            mutex(mutex) {
        B_ASSERT(mutex);

        b_mutex_lock(mutex);
    }

    B_MutexHolder(B_MutexHolder &&other) :
            B_MutexHolder(other.release()) {
    }

    ~B_MutexHolder() {
        if (mutex) {
            b_mutex_unlock(mutex);
        }
    }

    B_Mutex *
    release() {
        B_Mutex *mutex = this->mutex;
        this->mutex = nullptr;
        return mutex;
    }

private:
    B_Mutex *mutex;
};
#endif

#endif
