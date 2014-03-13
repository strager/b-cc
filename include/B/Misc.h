#ifndef B_HEADER_GUARD_F49AB188_F50C_428E_8F34_2DD311AD8AE5
#define B_HEADER_GUARD_F49AB188_F50C_428E_8F34_2DD311AD8AE5

#include <B/Base.h>
#include <B/Config.h>

#if defined(B_CONFIG_EPOLL)
# include <stdint.h>
# include <sys/epoll.h>
#endif

struct B_ErrorHandler;

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(B_CONFIG_EPOLL)
// epoll_ctl, setting the data to fd.
B_EXPORT_FUNC
b_epoll_ctl_fd(
        int epoll_fd,
        int operation,
        int fd,
        uint32_t events,
        struct B_ErrorHandler const *);
#endif

// Duplicates args such that a call to b_deallocate on the
// output pointer will deallocate the entire args array,
// including strings.
B_EXPORT_FUNC
b_dup_args(
        char const *const *args,
        B_OUTPTR char const *const **,
        struct B_ErrorHandler const *);

#if defined(__cplusplus)
}
#endif

#endif
