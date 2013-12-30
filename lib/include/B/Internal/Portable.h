#ifndef PORTABLE_H_E5B130AF_4834_4A83_BC1C_B543D7F8D37D
#define PORTABLE_H_E5B130AF_4834_4A83_BC1C_B543D7F8D37D

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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

void
b_create_thread(
    const char *thread_name,
    void (*thread_function)(void *user_closure),
    void *user_closure);

bool
b_current_thread_string(
    char *buffer,
    size_t buffer_size);

// For makecontext's callback.  Declares int parameters for
// B_UCONTEXT_POINTER.
#if defined(__ILP32__)
#define B_UCONTEXT_POINTER_PARAMS(prefix) \
    int prefix##0
#elif defined(__LP64__)
#define B_UCONTEXT_POINTER_PARAMS(prefix) \
    int prefix##0, int prefix##1
#else
#error Unsupported architecture
#endif

// For makecontext.  Reassembles a pointer from int
// arguments created with B_UCONTEXT_POINTER.
#if defined(__ILP32__)
#define B_UCONTEXT_POINTER(prefix) \
    ((void *) ((uintptr_t) prefix##0))
#elif defined(__LP64__)
#define B_UCONTEXT_POINTER(prefix) \
    ((void *) ( \
        ((uintptr_t) prefix##0) \
        | (((uintptr_t) prefix##1) << 32)))
#else
#error Unsupported architecture
#endif

// For makecontext.  Expands to an argument list (without
// parentheses) of integers from a pointer.
#if defined(__ILP32__)
#define B_UCONTEXT_POINTER_ARGS(value) \
    ((int) (intptr_t) (value))
#elif defined(__LP64__)
#define B_UCONTEXT_POINTER_ARGS(value) \
    ((int) (uintptr_t) (value)), \
    ((int) ((uintptr_t) (value) >> 32))
#else
#error Unsupported architecture
#endif

// For makecontext.  Number of arguments expanded by
// B_UCONTEXT_POINTER_ARGS.
#if defined(__ILP32__)
#define B_UCONTEXT_POINTER_ARG_COUNT 1
#elif defined(__LP64__)
#define B_UCONTEXT_POINTER_ARG_COUNT 2
#else
#error Unsupported architecture
#endif

#ifdef __cplusplus
}
#endif

#endif
