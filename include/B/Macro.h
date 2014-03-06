#ifndef B_HEADER_GUARD_DA2D6858_856A_4FAE_910D_EF12D34C6655
#define B_HEADER_GUARD_DA2D6858_856A_4FAE_910D_EF12D34C6655

#include <B/Config.h>

#if defined(__cplusplus)
# define B_COMPOUND_INIT_STRUCT(_struct_name, ...) \
    (_struct_name {__VA_ARGS__})
#else
# define B_COMPOUND_INIT_STRUCT(_struct_name, ...) \
    ((struct _struct_name) { __VA_ARGS__ })
#endif

#define B_HEX_DIGIT_TO_INT(x) \
    ('0' <= (x) && (x) <= '9' \
        ? (x) - '0' \
        : 'A' <= (x) && (x) <= 'F' \
        ? (x) - 'A' \
            : 'a' <= (x) && (x) <= 'f' \
            ? (x) - 'a' \
            : 0 /* FIXME(strager): Assert. */)

#if defined(B_CONFIG_GNU_ATTRIBUTES)
# define B_PRINTF_GNU_ATTRIBUTE(_type, _format, _params) \
    __attribute__((format(_type, _format, _params)))
#else
# define B_PRINTF_GNU_ATTRIBUTE(_type, _format, _params)
#endif

#endif
