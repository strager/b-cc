#ifndef B_HEADER_GUARD_D5820115_478D_4B20_B9E7_273CC9E8635C
#define B_HEADER_GUARD_D5820115_478D_4B20_B9E7_273CC9E8635C

#include <B/Base.h>
#include <B/Config.h>

#include <stdint.h>

#define B_ASSERTION_FAILURE(message) \
    (b_assertion_failure( \
        __func__, __FILE__, __LINE__, message))

#if defined(__clang__) || defined(__GNUC__)
# define B_ASSERT(_cond) \
    do { \
        if (!__builtin_expect(!!(_cond), 1)) { \
            B_ASSERTION_FAILURE(#_cond); \
        } \
    } while (0)
#else
# error "Unknown compiler"
#endif

#define B_BUG() B_ASSERTION_FAILURE("Bug!")
#define B_NYI() B_ASSERTION_FAILURE("Not yet implemented!")

// TODO(strager): Replace with __builtin_unreachable or
// whatever.
#define B_ASSERT_UNREACHABLE \
    B_ASSERT_UNREACHABLE("Unreachable!")

#if defined(B_CONFIG_CXX_STATIC_ASSERT)
# define B_STATIC_ASSERT(_cond, _message) \
    static_assert(_cond, _message)
#elif defined(B_CONFIG_C_STATIC_ASSERT)
# define B_STATIC_ASSERT(_cond, _message) \
    _Static_assert(_cond, _message)
#else
# define B_STATIC_ASSERT(_cond, _message) \
    extern void b_static_assert_failure_( \
            int _[(_cond) ? 1 : -1])
#endif

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(__clang__) || defined(__GNUC__)
__attribute__((noreturn))
#endif
B_EXPORT void
b_assertion_failure(
        B_OPT char const *function,
        B_OPT char const *file,
        int64_t line,
        char const *message);

#if defined(__cplusplus)
}
#endif

#endif
