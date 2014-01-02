#ifndef PORTABLE_H_E5B130AF_4834_4A83_BC1C_B543D7F8D37D
#define PORTABLE_H_E5B130AF_4834_4A83_BC1C_B543D7F8D37D

#include <B/Internal/Common.h>

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

char *
b_strdup(
    const char *);

char *
b_memdup(
    const char *,
    size_t);

void *
b_reallocf(
    void *,
    size_t);

size_t
b_min_size(
    size_t,
    size_t);

B_ERRFUNC
b_create_thread(
    const char *thread_name,
    void (*thread_function)(void *user_closure),
    void *user_closure);

bool
b_current_thread_string(
    char *buffer,
    size_t buffer_size);

#ifdef __cplusplus
}
#endif

#if defined(__cplusplus)
#include <functional>
#include <memory>

template<typename TFunc>
B_ERRFUNC
b_create_thread(
    char const *thread_name,
    TFunc const &thread_function) {

    return b_create_thread(
        thread_name,
        [](void *closure) {
            auto thread_function_ptr
                = std::unique_ptr<std::function<void ()>>(
                    static_cast<std::function<void ()> *>(closure));
            (*thread_function_ptr)();
        },
        new std::function<void ()>(thread_function));
}
#endif

#endif
