#include <B/Assert.h>
#include <B/Config.h>
#include <B/Log.h>

#include <stdio.h>

#if defined(B_CONFIG_PTHREAD)
# include <pthread.h>
#endif

#if defined(__MACH__)
# include <mach/mach_init.h>
#endif

#define B_LOG_FILE stderr

#if defined(B_CONFIG_PTHREAD)
static pthread_mutex_t
s_log_lock_ = PTHREAD_MUTEX_INITIALIZER;
#endif

static char const *
log_level_name_(
        enum B_LogLevel log_level) {
    switch (log_level) {
    case B_DEBUG: return "Debug";
    }
    B_BUG();
}

B_EXPORT void
b_log_format_impl(
        enum B_LogLevel log_level,
        char const *file_name,
        size_t line_number,  // 1-based
        char const *format,
        ...) {
    bool acquired_lock = b_log_lock();

    // Log level.
    (void) fprintf(
        B_LOG_FILE,
        "[%s ",
        log_level_name_(log_level));

    // Thread ID.
    // TODO(strager): Print thread name as well.
#if defined(__MACH__)
    // TODO(strager): Use a direct Mach API.
    (void) fprintf(
        B_LOG_FILE,
        "(%u) ",
        mach_thread_self());
#elif defined(B_CONFIG_PTHREAD)
    // FIXME(strager): Not portable (e.g. fails on
    // pthread-win32).
    (void) fprintf(
        B_LOG_FILE,
        "(%p) ",
        (void *) pthread_self());
#endif

    // File name, line number.
    // FIXME(strager): %zu is non-portable.
    (void) fprintf(
        B_LOG_FILE,
        "%s:%zu]: ",
        file_name,
        line_number);

    // Message.
    va_list args;
    va_start(args, format);
    (void) vfprintf(B_LOG_FILE, format, args);
    va_end(args);

    (void) fprintf(B_LOG_FILE, "\n");
    (void) fflush(B_LOG_FILE);

    if (acquired_lock) {
        b_log_unlock();
    }
}

B_EXPORT void
b_log_format_raw_locked(
        char const *format,
        ...) {
    va_list args;
    va_start(args, format);
    (void) vfprintf(B_LOG_FILE, format, args);
    va_end(args);
}

B_EXPORT bool
b_log_lock() {
#if defined(B_CONFIG_PTHREAD)
    int rc = pthread_mutex_lock(&s_log_lock_);
    return rc == 0;
#else
    return false;
#endif
}

B_EXPORT void
b_log_unlock() {
#if defined(B_CONFIG_PTHREAD)
    int rc = pthread_mutex_unlock(&s_log_lock_);
    B_ASSERT(rc == 0);
#endif
}
