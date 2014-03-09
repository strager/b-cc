#ifndef B_HEADER_GUARD_159DBF52_11F4_4B37_81EA_A36931CC655C
#define B_HEADER_GUARD_159DBF52_11F4_4B37_81EA_A36931CC655C

#include <B/Base.h>

#include <errno.h>

// FIXME(strager)
#if defined(EPERM)
# define B_ERROR_ALREADY_CANCELLED EPERM
# define B_ERROR_ALREADY_COMPLETED EPERM
#else
# define B_ERROR_ALREADY_CANCELLED EINVAL
# define B_ERROR_ALREADY_COMPLETED EINVAL
#endif

#endif