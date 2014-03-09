#include <B/Config.h>
#include <B/Misc.h>

#if defined(B_CONFIG_EPOLL)
# include <B/Error.h>

# include <errno.h>
# include <stdint.h>
# include <string.h>
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

#if !defined(B_CONFIG_EPOLL)
// HACK(strager): We must have an export from this TU.
B_EXPORT_FUNC
b_misc_dummy_export_(void) {
    return false;
}
#endif
