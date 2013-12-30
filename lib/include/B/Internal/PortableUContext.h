#ifndef PORTABLEUCONTEXT_H_57A8A442_D623_4C0C_BD64_EA2A0E092519
#define PORTABLEUCONTEXT_H_57A8A442_D623_4C0C_BD64_EA2A0E092519

#include <B/Internal/Common.h>

#include <stddef.h>
#include <stdint.h>

struct B_UContext;

#if defined(B_UCONTEXT_IMPL)
struct B_UContextOpaque {
#else
struct B_UContext {
#endif
#if defined(__x86_64__) && defined(__LP64__)
    uint8_t bytes[72];
#else
#error Unsupported architecture
#endif
};

void
b_ucontext_getcontext(
    struct B_UContext *);

void B_NO_RETURN
b_ucontext_setcontext(
    struct B_UContext const *);

void
b_ucontext_makecontext(
    struct B_UContext *,
    void *stack,
    size_t stack_size,
    void B_NO_RETURN (*callback)(void *user_closure),
    void *user_closure);

void
b_ucontext_swapcontext(
    struct B_UContext *from,
    struct B_UContext const *to);

void
b_ucontext_copy(
    struct B_UContext *dest,
    struct B_UContext const *source);

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

#endif
