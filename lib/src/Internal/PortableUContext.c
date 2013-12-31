#define B_UCONTEXT_IMPL

#include <B/Internal/PortableUContext.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <stdio.h>

// b_ucontext_setcontext and b_ucontext_getcontext are
// implemented in assembly in PortableUContextASM.S.

#if defined(__x86_64__) && defined(__LP64__)

#if !defined(__APPLE__)
// TODO(strager): Whitelist other platforms (Linux, Android,
// FreeBSD?).
#error Only System V AMD64 ABI is supported
#endif

// See System V AMD64 ABI:
// http://x86-64.org/documentation/abi.pdf
//
// Registers which need to be saved are documented in figure
// 3.4 Register Usage, page 21.
struct B_UContext {
    uint64_t rbx;  // 0
    uint64_t rsp;  // 8
    uint64_t rbp;  // 16
    uint64_t r12;  // 24
    uint64_t r13;  // 32
    uint64_t r14;  // 40
    uint64_t r15;  // 48
    uint64_t rip;  // 56

    // TODO(strager): x87 control word.

    // Used only for the first argument of the
    // b_ucontext_makecontext callback (void *user_context).
    uint64_t rdi;  // 64
};

_Static_assert(
    sizeof(struct B_UContext)
    == sizeof(struct B_UContextOpaque),
    "B_UContextOpaque must match B_UContext");

void
b_ucontext_makecontext(
    struct B_UContext *context,
    void *stack,
    size_t stack_size,
    void (*callback)(void *user_closure),
    void *user_closure) {

    memset(context, 0, sizeof(*context));

    // Stack (grows down).
    uintptr_t sp = (uintptr_t) stack + stack_size;
#if defined(__APPLE__)
    // Align down to 16 bytes.
    sp &= ~0xF;
#endif

    // Return address.
    sp -= sizeof(uint64_t);
    *(void (**)(void)) sp = abort;

    printf(
        "makecontext stack=%p sp=%p callback=%p user_closure=%p\n",
        stack,
        (void *) sp,
        callback,
        user_closure);

    // Arguments.
    context->rdi = (uint64_t) (uintptr_t) user_closure;

    context->rip = (uint64_t) (uintptr_t) callback;
    context->rsp = (uint64_t) sp;
}

void
b_ucontext_swapcontext(
    struct B_UContext *from,
    struct B_UContext const *to) {

    volatile bool did_swap = false;
    b_ucontext_getcontext(from);
    if (!did_swap) {
        did_swap = true;
        b_ucontext_setcontext(to);
#if defined(B_DEBUG)
        abort();
#endif
    }
}

void
b_ucontext_copy(
    struct B_UContext *dest,
    struct B_UContext const *source) {

    memcpy(dest, source, sizeof(struct B_UContext));
}
#else
#error Unsupported architecture
#endif
