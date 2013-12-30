#ifndef PORTABLEUCONTEXT_H_57A8A442_D623_4C0C_BD64_EA2A0E092519
#define PORTABLEUCONTEXT_H_57A8A442_D623_4C0C_BD64_EA2A0E092519

#if defined(__APPLE__) && !defined(_XOPEN_SOURCE)
# if defined(_STRUCT_UCONTEXT)
#  error ucontext_t already defined; please include PortableUContext.h before other headers
# endif

# if !defined(_DARWIN_C_SOURCE)
#  define _DARWIN_C_SOURCE
#  define B_DEFINED_DARWIN_C_SOURCE
# endif
# define _XOPEN_SOURCE 600

# include <ucontext.h>

# undef _XOPEN_SOURCE
// TODO(strager): Figure out why we must leak
// _DARWIN_C_SOURCE.
//# if defined(B_DEFINED_DARWIN_C_SOURCE)
//#  undef _DARWIN_C_SOURCE
//# endif

_Static_assert(
    sizeof(((ucontext_t *) 0)->__mcontext_data) == sizeof(_STRUCT_MCONTEXT),
    "ucontext_t should have __mcontext_data member");

// Sorry!  Apple says ucontext functions are deprecated.
# pragma clang diagnostic ignored "-Wdeprecated-declarations"
#else
# include <ucontext.h>
#endif

#include <stdint.h>

void
b_ucontext_init(
    ucontext_t *);

void
b_ucontext_copy(
    ucontext_t *dest,
    ucontext_t const *source);

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
