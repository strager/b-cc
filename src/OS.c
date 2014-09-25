// glibc requires _POSIX_C_SOURCE >= 1 for SSIZE_MAX.  Alas,
// there is no macro to check for glibc before setting
// _POSIX_C_SOURCE, so we do it regardless of whether we are
// on glibc or not.
#if !defined(_POSIX_C_SOURCE)
# define _POSIX_C_SOURCE 1
#endif
#include <limits.h>

#include <B/Assert.h>
#include <B/Config.h>
#include <B/Error.h>
#include <B/Private/OS.h>

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#if defined(B_CONFIG_EPOLL)
# include <sys/epoll.h>
# include <sys/eventfd.h>
# include <sys/signalfd.h>
#endif

#define B_CATCH_ERROR_IF_(condition_, function_name_) \
    do { \
        if ((condition_)) { \
            switch (B_RAISE_ERRNO_ERROR( \
                    eh, errno, function_name_)) { \
            case B_ERROR_ABORT: \
            case B_ERROR_IGNORE: \
                return false; \
            case B_ERROR_RETRY: \
                goto retry; \
            } \
        } \
    } while (0)

#if defined(B_CONFIG_KQUEUE)
B_EXPORT_FUNC
b_kqueue(
        B_OUT int *fd,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, fd);
retry:;
    int rc = kqueue();
    B_CATCH_ERROR_IF_(rc == -1, "kqueue");
    *fd = rc;
    return true;
}
#endif

#if defined(B_CONFIG_EPOLL)
B_EXPORT_FUNC
b_epoll_create1(
        int flags,
        B_OUT int *fd,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, fd);
retry:;
    int rc = epoll_create1(flags);
    B_CATCH_ERROR_IF_(rc == -1, "epoll_create1");
    *fd = rc;
    return true;
}

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
        epoll_fd, operation, fd, &event);
    B_CATCH_ERROR_IF_(rc == -1, "epoll_ctl");
    return true;
}

B_EXPORT_FUNC
b_eventfd(
        unsigned int initial_value,
        int flags,
        B_OUT int *fd,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, fd);
retry:;
    int rc = eventfd(initial_value, flags);
    B_CATCH_ERROR_IF_(rc == -1, "eventfd");
    *fd = rc;
    return true;
}

B_EXPORT_FUNC
b_eventfd_read(
        int fd,
        B_OUT eventfd_t *value,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, value);
retry:;
    int rc = eventfd_read(fd, value);
    B_CATCH_ERROR_IF_(rc == -1, "eventfd_read");
    return true;
}

B_EXPORT_FUNC
b_eventfd_write(
        int fd,
        eventfd_t value,
        struct B_ErrorHandler const *eh) {
retry:;
    int rc = eventfd_write(fd, value);
    B_CATCH_ERROR_IF_(rc == -1, "eventfd_write");
    return true;
}

B_EXPORT_FUNC
b_signalfd_create(
        const sigset_t *mask,
        int flags,
        B_OUT int *fd,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, mask);
    B_CHECK_PRECONDITION(eh, fd);
retry:;
    int rc = signalfd(-1, mask, flags);
    B_CATCH_ERROR_IF_(rc == -1, "eventfd");
    *fd = rc;
    return true;
}
#endif

B_EXPORT_FUNC
b_map_file_by_path(
        B_FilePath const *path,
        B_OUTPTR void **out_data,
        B_OUT size_t *out_data_size,
        struct B_ErrorHandler const *eh) {
    // TODO(strager): B_CHECK_PRECONDITION

    int fd = -1;
    void *data = MAP_FAILED;

    off_t file_size;
    fd = open(path, O_RDONLY);
    if (fd == -1) {
        B_RAISE_ERRNO_ERROR(eh, errno, "open");
        goto fail;
    }

    file_size = lseek(fd, 0, SEEK_END);
    if (file_size == -1) {
        B_RAISE_ERRNO_ERROR(eh, errno, "lseek");
        goto fail;
    }
    B_ASSERT(file_size >= 0);
    if (lseek(fd, 0, SEEK_SET) == -1) {
        B_RAISE_ERRNO_ERROR(eh, errno, "lseek");
        goto fail;
    }

    // Don't allocate more bytes than size_t can represent.
    B_STATIC_ASSERT(
        SSIZE_MAX <= SIZE_MAX,
        "SSIZE_MAX must be <= SIZE_MAX");
    if (file_size > SSIZE_MAX) {
        B_RAISE_ERRNO_ERROR(
            eh, ENOMEM, "b_map_file_by_path");
        goto fail;
    }

    // FIXME(strager): What about files of size 0?
    B_ASSERT(file_size != 0);

    data = mmap(
        NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) {
        B_RAISE_ERRNO_ERROR(eh, errno, "mmap");
        goto fail;
    }

    // Close the file descriptor.  We don't have a way to
    // return the file descriptor so the caller can close
    // it.  The memory mapping will stay alive even with the
    // file descriptor closed.
    if (close(fd) == -1) {
        (void) B_RAISE_ERRNO_ERROR(eh, errno, "close");
        goto fail;
    }
    fd = -1;

    *out_data = data;
    *out_data_size = file_size;
    return true;

fail:
    if (data != MAP_FAILED) {
        if (munmap(data, file_size) == -1) {
            (void) B_RAISE_ERRNO_ERROR(eh, errno, "munmap");
        }
    }
    if (fd != -1) {
        if (close(fd) == -1) {
            (void) B_RAISE_ERRNO_ERROR(eh, errno, "close");
        }
    }
    return false;
}

B_EXPORT_FUNC
b_unmap_file(
        void *data,
        size_t data_size,
        struct B_ErrorHandler const *eh) {
    // TODO(strager): B_CHECK_PRECONDITION

    // TODO(strager): In debug mode, keep track of mappings
    // to ensure munmap isn't called on random pages.
    if (munmap(data, data_size) == -1) {
        (void) B_RAISE_ERRNO_ERROR(eh, errno, "munmap");
        return false;
    }
    return true;
}
