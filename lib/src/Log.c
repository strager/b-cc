#include <B/Exception.h>
#include <B/Fiber.h>
#include <B/Internal/Portable.h>
#include <B/Log.h>

#include <assert.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

#define B_LOG_FILE stderr

static pthread_mutex_t
log_mutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_mutex_t
log_user_mutex = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER;

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

    rc = pthread_mutex_lock(&log_mutex);
    assert(rc == 0);

    // Print header.
    // TODO(strager): Print log level.
    rc = fprintf(B_LOG_FILE, "[%s", thread_buffer);
    assert(rc >= 0);

    {
        // Print fiber ID, if available.
        void *fiber_id;
        struct B_Exception *ex
            = b_fiber_context_current_fiber_id(
                fiber_context,
                &fiber_id);
        if (!ex && fiber_id) {
            rc = fprintf(B_LOG_FILE, ".%p", fiber_id);
            assert(rc >= 0);
        }
        if (ex) {
            b_exception_deallocate(ex);
        }
    }

    rc = fprintf(B_LOG_FILE, "]: ");
    assert(rc >= 0);

    va_list args;
    va_start(args, format);
    rc = vfprintf(B_LOG_FILE, format, args);
    va_end(args);
    assert(rc >= 0);

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
