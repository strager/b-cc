#ifndef B_HEADER_GUARD_288D02F0_9345_4EE6_80CA_73BE6706ACF5
#define B_HEADER_GUARD_288D02F0_9345_4EE6_80CA_73BE6706ACF5

#include <B/Base.h>
#include <B/Config.h>
#include <B/FilePath.h>

#include <stddef.h>

#if defined(B_CONFIG_KQUEUE)
# include <sys/event.h>
#endif

#if defined(B_CONFIG_EPOLL)
# include <signal.h>
# include <stdint.h>
# include <sys/epoll.h>
# include <sys/eventfd.h>
#endif

struct B_ErrorHandler;

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(B_CONFIG_KQUEUE)
B_EXPORT_FUNC
b_kqueue(
        B_OUT int *fd,
        struct B_ErrorHandler const *);
#endif

#if defined(B_CONFIG_EPOLL)
B_EXPORT_FUNC
b_epoll_create1(
        int flags,
        B_OUT int *fd,
        struct B_ErrorHandler const *);

// epoll_ctl, setting the data to fd.
B_EXPORT_FUNC
b_epoll_ctl_fd(
        int epoll_fd,
        int operation,
        int fd,
        uint32_t events,
        struct B_ErrorHandler const *);

B_EXPORT_FUNC
b_eventfd(
        unsigned int initial_value,
        int flags,
        B_OUT int *fd,
        struct B_ErrorHandler const *);

B_EXPORT_FUNC
b_eventfd_read(
        int fd,
        B_OUT eventfd_t *value,
        struct B_ErrorHandler const *);

B_EXPORT_FUNC
b_eventfd_write(
        int fd,
        eventfd_t value,
        struct B_ErrorHandler const *);

// signalfd with -1 fd.
B_EXPORT_FUNC
b_signalfd_create(
        const sigset_t *mask,
        int flags,
        B_OUT int *fd,
        struct B_ErrorHandler const *);
#endif

#if defined(B_CONFIG_POSIX_FD)
B_EXPORT_FUNC
b_close_fd(
        int fd,
        struct B_ErrorHandler const *);
#endif

// Maps a file into memory, a la MapViewOfFile or mmap.
B_EXPORT_FUNC
b_map_file_by_path(
        B_FilePath const *path,
        B_OUTPTR void **data,
        B_OUT size_t *data_size,
        struct B_ErrorHandler const *);

// Unmaps a file from memory, a la UnmapViewOfFile or
// munmap.  The given pointer and size must exactly match
// the results of b_map_file_by_path.
B_EXPORT_FUNC
b_unmap_file(
        void *data,
        size_t data_size,
        struct B_ErrorHandler const *);

#if defined(__cplusplus)
}
#endif

#endif
