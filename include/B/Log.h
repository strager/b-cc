#ifndef B_HEADER_GUARD_014F3198_BDF0_4F9A_8FB0_85755E632BEA
#define B_HEADER_GUARD_014F3198_BDF0_4F9A_8FB0_85755E632BEA

#include <B/Base.h>
#include <B/Macro.h>

#include <stdbool.h>
#include <stddef.h>

enum B_LogLevel {
    B_DEBUG = 1,
};

#define B_LOG(_level, ...) \
    b_log_format_impl( \
        (_level), \
        __FILE__, \
        __LINE__, \
        __VA_ARGS__)

#if defined(__cplusplus)
extern "C" {
#endif

B_EXPORT void
b_log_format_impl(
        enum B_LogLevel,
        char const *file_name,
        size_t line_number,  // 1-based
        char const *format,
        ...)
    B_PRINTF_GNU_ATTRIBUTE(printf, 4, 5);

B_EXPORT void
b_log_format_raw_locked(
        char const *format,
        ...)
    B_PRINTF_GNU_ATTRIBUTE(printf, 1, 2);

B_EXPORT bool
b_log_lock();

B_EXPORT void
b_log_unlock();

#if defined(__cplusplus)
}
#endif

#endif
