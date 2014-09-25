// glibc requires _POSIX_C_SOURCE >= 1 for SSIZE_MAX.  Alas,
// there is no macro to check for glibc before setting
// _POSIX_C_SOURCE, so we do it regardless of whether we are
// on glibc or not.
#if !defined(_POSIX_C_SOURCE)
# define _POSIX_C_SOURCE 1
#endif
#include <limits.h>

#include <B/Alloc.h>
#include <B/Assert.h>
#include <B/Config.h>
#include <B/Error.h>
#include <B/Private/Misc.h>

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#if defined(B_CONFIG_EPOLL)
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
        epoll_fd, operation, fd, &event);
    if (rc == -1) {
        switch (B_RAISE_ERRNO_ERROR(
                eh, errno, "epoll_ctl")) {
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

B_EXPORT_FUNC
b_map_file_by_path(
        B_FilePath const *path,
        B_OUTPTR void **out_data,
        B_OUT size_t *out_data_size,
        struct B_ErrorHandler const *eh) {
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
    // TODO(strager): In debug mode, keep track of mappings
    // to ensure munmap isn't called on random pages.
    if (munmap(data, data_size) == -1) {
        (void) B_RAISE_ERRNO_ERROR(eh, errno, "munmap");
        return false;
    }
    return true;
}
