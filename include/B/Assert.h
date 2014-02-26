#ifndef B_HEADER_GUARD_D5820115_478D_4B20_B9E7_273CC9E8635C
#define B_HEADER_GUARD_D5820115_478D_4B20_B9E7_273CC9E8635C

#include <assert.h>

#define B_ASSERT(_cond) assert(_cond)

#define B_BUG() assert(0 && "Bug!")

#if defined(__cplusplus)
# define B_STATIC_ASSERT(_cond, _message) \
    static_assert(_cond, _message)
#else
# define B_STATIC_ASSERT(_cond, _message) \
    _Static_assert(_cond, _message)
#endif

#endif
