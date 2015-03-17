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

#endif
