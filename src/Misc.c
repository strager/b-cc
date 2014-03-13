#include <B/Alloc.h>
#include <B/Assert.h>
#include <B/Config.h>
#include <B/Error.h>
#include <B/Misc.h>
#include <string.h>

#if defined(B_CONFIG_EPOLL)
# include <errno.h>
# include <stdint.h>
# include <sys/epoll.h>

// epoll_ctl, setting the data to fd.
B_EXPORT_FUNC
b_epoll_ctl_fd(
        int epoll_fd,
        int operation,
        int fd,
        uint32_t events,
        struct B_ErrorHandler const *eh) {
    struct epoll_event event;
    // NOTE(strager): Needed for Valgrind.
    memset(&event, 0, sizeof(event));
    event.events = events;
    event.data.fd = fd;

retry:;
    int rc = epoll_ctl(
        epoll_fd,
        operation,
        fd,
        &event);
    if (rc == -1) {
        switch (B_RAISE_ERRNO_ERROR(
                eh,
                errno,
                "epoll_ctl")) {
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

B_EXPORT_FUNC
b_dup_args(
        char const *const *args,
        B_OUTPTR char const *const **out,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, args);
    B_CHECK_PRECONDITION(eh, out);

    size_t arg_count = 0;    // Excludes NULL terminator.
    size_t args_length = 0;  // Excludes \0 terminators.
    for (char const *const *p = args; *p; ++p) {
        arg_count += 1;
        args_length += strlen(*p);
    }

    // Layout:
    // 0000: 0008  // Pointer to "echo".
    // 0002: 000D  // Pointer to "hello".
    // 0004: 0013  // Pointer to "world".
    // 0006: 0000  // NULL terminator for args.
    // 0008: "echo\0"
    // 000D: "hello\0"
    // 0013: "world\0"
    // 0019: <end of buffer>
    size_t arg_pointers_size
        = sizeof(char const *) * (arg_count + 1);
    size_t arg_strings_size = args_length + arg_count;
    size_t arg_buffer_size
        = arg_pointers_size + arg_strings_size;
    char const *const *args_buffer;
    if (!b_allocate(
            arg_buffer_size,
            (void **) &args_buffer,
            eh)) {
        return false;
    }
    void *args_buffer_end
        = (char *) args_buffer + arg_buffer_size;

    char const **out_arg_pointer
        = (char const **) args_buffer;
    char *out_arg_string
        = (char *) args_buffer + arg_pointers_size;
    for (char const *const *p = args; *p; ++p) {
        *out_arg_pointer++ = out_arg_string;
        size_t p_size = strlen(*p) + 1;
        B_ASSERT(out_arg_string + p_size
            <= (char *) args_buffer_end);
        memcpy(out_arg_string, *p, p_size);
        B_ASSERT(out_arg_string[p_size - 1] == '\0');
        out_arg_string += p_size;
    }
    *out_arg_pointer++ = NULL;
    B_ASSERT(out_arg_string == (char *) args_buffer_end);
    B_ASSERT((char *) out_arg_pointer
        == (char *) args_buffer + arg_pointers_size);

    *out = args_buffer;
    return true;
}
