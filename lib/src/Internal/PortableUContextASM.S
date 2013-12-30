#define B_PASTE_RAW(x,y) x##y
#define B_PASTE(x,y) B_PASTE_RAW(x,y)
#define B_LABEL(x) B_PASTE(__USER_LABEL_PREFIX__,x)

.text

#if defined(__x86_64__) && defined(__LP64__)
.p2align 2
.global B_LABEL(b_ucontext_setcontext)
B_LABEL(b_ucontext_setcontext):
    movq  0(%rdi), %rbx
    movq 16(%rdi), %rbp
    movq 24(%rdi), %r12
    movq 32(%rdi), %r13
    movq 40(%rdi), %r14
    movq 48(%rdi), %r15

    movq 8(%rdi), %rsp
    pushq 56(%rdi)  // rip
    movq 64(%rdi), %rdi
    ret  // pop %rip
#else
#error Unsupported architecture
#endif
