#include <B/Exception.h>
#include <B/Fiber.h>
#include <B/Internal/Portable.h>
#include <B/Internal/Util.h>
#include <B/Log.h>

#include <assert.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

#define B_LOG_FILE stderr

#define B_LOG_CONSTANT(string) \
    do { \
        int rc = fwrite( \
            "" string "", \
            sizeof("" string "") - 1, \
            1, \
            B_LOG_FILE); \
        assert(rc == 1); \
    } while (0)

static pthread_mutex_t
log_mutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_mutex_t
log_user_mutex = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER;

// TODO(strager): Make these configurable.
static bool const
use_ansi = true;

static bool const
use_thread_colors = true;

bool
b_log_is_level_enabled(
    enum B_LogLevel log_level) {

    switch (log_level) {
    case B_ZMQ:       return false;
    case B_FIBER:     return true;
    case B_INFO:      return true;
    case B_EXCEPTION: return true;
    default:          return false;
    }
}

static void
b_log_ansi_start_level(
    enum B_LogLevel log_level) {

    switch (log_level) {
    case B_ZMQ:       B_LOG_CONSTANT("\x1B[32m"); return;
    case B_FIBER:     B_LOG_CONSTANT("\x1B[34m"); return;
    case B_INFO:      return;
    case B_EXCEPTION: B_LOG_CONSTANT("\x1B[31m"); return;
    default:          return;
    }
}

static void
b_log_ansi_end_level(
    enum B_LogLevel log_level) {

    switch (log_level) {
    case B_ZMQ:
    case B_FIBER:
    case B_EXCEPTION:
        B_LOG_CONSTANT("\x1B[39m");
        return;

    case B_INFO:
    default:
        return;
    }
}

static void
b_log_ansi_start_thread(
    char const *thread_name,
    void *fiber_id) {

    // Hash the string to get an arbitrary but consistent
    // colour.
    uint32_t hash = 0;
    for (char const *c = thread_name; *c; ++c) {
        hash = b_hash_add_8(hash, *c);
    }
    hash = b_hash_add_pointer(hash, fiber_id);

    uint8_t r = hash % 6;
    uint8_t g = hash / 6 % 6;
    uint8_t b = hash / 6 / 6 % 6;

    int color_index
        = 16
        + (r * 36)
        + (g * 6)
        + b;

    // TODO(strager): Shy away from grayscale and dark/light
    // colours.

    int rc = fprintf(
        B_LOG_FILE,
        "\x1B[38;5;%dm",
        color_index);
    assert(rc >= 0);
}

static void
b_log_ansi_end_thread(
    char const *thread_name,
    void *fiber_id) {

    (void) thread_name;
    (void) fiber_id;

    B_LOG_CONSTANT("\x1B[39m");
}

void
b_log_format(
    enum B_LogLevel log_level,
    struct B_FiberContext *fiber_context,
    const char *format,
    ...) {

    if (!b_log_is_level_enabled(log_level)) {
        return;
    }

    int rc;

    char thread_buffer[64];
    bool ok = b_current_thread_string(
        thread_buffer,
        sizeof(thread_buffer));
    (void) ok;  // Allow truncation.

    // Get fiber ID, if available.
    void *fiber_id = NULL;
    {
        struct B_Exception *ex
            = b_fiber_context_current_fiber_id(
                fiber_context,
                &fiber_id);
        if (ex) {
            b_exception_deallocate(ex);
        }
    }

    rc = pthread_mutex_lock(&log_mutex);
    assert(rc == 0);

    // Print start of colour.
    if (use_ansi) {
        if (use_thread_colors) {
            b_log_ansi_start_thread(
                thread_buffer,
                fiber_id);
        } else {
            b_log_ansi_start_level(log_level);
        }
    }

    // Print header.
    // TODO(strager): Print log level.
    rc = fprintf(B_LOG_FILE, "[%s", thread_buffer);
    assert(rc >= 0);

    if (fiber_id) {
        rc = fprintf(B_LOG_FILE, ".%p", fiber_id);
        assert(rc >= 0);
    }

    rc = fprintf(B_LOG_FILE, "]: ");
    assert(rc >= 0);

    // Print message.
    va_list args;
    va_start(args, format);
    rc = vfprintf(B_LOG_FILE, format, args);
    va_end(args);
    assert(rc >= 0);

    // Print end of colour.
    if (use_ansi) {
        if (use_thread_colors) {
            b_log_ansi_end_thread(
                thread_buffer,
                fiber_id);
        } else {
            b_log_ansi_end_level(log_level);
        }
    }

    rc = fputc('\n', B_LOG_FILE);
    assert(rc >= 0);

    rc = pthread_mutex_unlock(&log_mutex);
    assert(rc == 0);
}

void
b_log_exception(
    struct B_Exception const *ex) {

    char const *message = b_exception_allocate_message(ex);
    b_log_format(B_EXCEPTION, NULL, "%s", message);
    b_exception_deallocate_message(ex, message);
}

void
b_log_lock(
    void) {

    int rc = pthread_mutex_lock(&log_user_mutex);
    assert(rc == 0);
}

void
b_log_unlock(
    void) {

    int rc = pthread_mutex_unlock(&log_user_mutex);
    assert(rc == 0);
}
