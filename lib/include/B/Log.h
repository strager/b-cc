#ifndef LOG_H_A5487636_4D6E_4DBB_B9CF_6E83A6C1C355
#define LOG_H_A5487636_4D6E_4DBB_B9CF_6E83A6C1C355

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

struct B_Exception;
struct B_FiberContext;

enum B_LogLevel {
    B_ZMQ,
    B_FIBER,
    B_INFO,
    B_EXCEPTION,
};

bool
b_log_is_level_enabled(
    enum B_LogLevel);

void
b_log_format(
    enum B_LogLevel,
    struct B_FiberContext *,
    const char *format,
    ...);

void
b_log_exception(
    struct B_Exception const *);

void
b_log_lock(
    void);

void
b_log_unlock(
    void);

#define B_LOG(level, ...) \
    b_log_format(level, NULL, __VA_ARGS__)

#define B_LOG_FIBER(level, fiber_context, ...) \
    b_log_format(level, fiber_context, __VA_ARGS__)

#define B_LOG_EXCEPTION(ex) \
    b_log_exception(ex)

#ifdef __cplusplus
}
#endif

#endif
