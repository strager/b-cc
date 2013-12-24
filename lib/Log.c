#include "Exception.h"
#include "Log.h"
#include "Portable.h"

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
    case B_INFO:      return false;
    case B_EXCEPTION: return true;
    default:          return false;
    }
}

void
b_log_format(
    enum B_LogLevel log_level,
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

    pthread_mutex_lock(&log_mutex);

    // Print header.
    // TODO(strager): Print log level.
    rc = fprintf(B_LOG_FILE, "[%s]: ", thread_buffer);
    if (rc < 0) {
        // TODO(strager): Do something!
    }

    va_list args;
    va_start(args, format);
    rc = vfprintf(B_LOG_FILE, format, args);
    va_end(args);
    if (rc < 0) {
        // TODO(strager): Do something!
    }

    rc = fputc('\n', B_LOG_FILE);
    if (rc < 0) {
        // TODO(strager): Do something!
    }

    pthread_mutex_unlock(&log_mutex);
}

void
b_log_exception(
    struct B_Exception const *ex) {

    b_log_format(B_EXCEPTION, "%s", ex->message);
}

void
b_log_lock(
    void) {

    pthread_mutex_lock(&log_user_mutex);
}

void
b_log_unlock(
    void) {

    pthread_mutex_unlock(&log_user_mutex);
}
