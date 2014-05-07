#ifndef B_HEADER_GUARD_D5820115_478D_4B20_B9E7_273CC9E8635C
#define B_HEADER_GUARD_D5820115_478D_4B20_B9E7_273CC9E8635C

#include <assert.h>

#define B_ASSERT(_cond) assert(_cond)

#define B_BUG() assert(0 && "Bug!")
#define B_NYI() assert(0 && "Not yet implemented!")

// TODO(strager): Replace with __builtin_unreachable or
// whatever.
#define B_ASSERT_UNREACHABLE assert(0 && "Unreachable!")

#if defined(__cplusplus)
# define B_STATIC_ASSERT(_cond, _message) \
    static_assert(_cond, _message)
#else
# define B_STATIC_ASSERT(_cond, _message) \
    _Static_assert(_cond, _message)
#endif

#if defined(__cplusplus)
# include <type_traits>
# define B_STATIC_ASSERT_MUST_OVERRIDE(_subtype, _member) \
    B_STATIC_ASSERT( \
        (!std::is_same< \
            decltype(&std::remove_reference< \
                decltype(*this)>::type::_member), \
            decltype(&T::_member)>::value), \
        "Must override " #_member " in subtype")
# define B_ASSERT_MUST_OVERRIDE(_subtype, _member) \
    do { \
        B_STATIC_ASSERT_MUST_OVERRIDE(_subtype, _member); \
        B_ASSERT_UNREACHABLE(); \
    } while (0)
#endif

#endif
