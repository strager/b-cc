#ifndef B_HEADER_GUARD_DA2D6858_856A_4FAE_910D_EF12D34C6655
#define B_HEADER_GUARD_DA2D6858_856A_4FAE_910D_EF12D34C6655

#include <B/Config.h>

#if defined(B_CONFIG_GNU_ATTRIBUTES)
# define B_PRINTF_GNU_ATTRIBUTE(_type, _format, _params) \
    __attribute__((format(_type, _format, _params)))
#else
# define B_PRINTF_GNU_ATTRIBUTE(_type, _format, _params)
#endif

#if defined(B_CONFIG_NO_CONST_STRUCT_MEMBERS)
# define B_CONST_STRUCT_MEMBER
#else
# define B_CONST_STRUCT_MEMBER const
#endif

// Clang's -Wconditional-uninitialized is a bit strict.
#if defined(__clang__)
# define B_BEGIN_IGNORE_CONDITIONAL_UNINITIALIZED \
    _Pragma("GCC diagnostic push") \
    _Pragma("GCC diagnostic ignored \"-Wconditional-uninitialized\"")
# define B_END_IGNORE_CONDITIONAL_UNINITIALIZED \
    _Pragma("GCC diagnostic pop")
#else
# define B_BEGIN_IGNORE_CONDITIONAL_UNINITIALIZED
# define B_END_IGNORE_CONDITIONAL_UNINITIALIZED
#endif

#endif
