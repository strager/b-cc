#if defined(__linux__)
# define _GNU_SOURCE
#endif

#include <B/Assert.h>
#include <B/Config.h>
#include <B/Log.h>

#include <stdarg.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>

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

#if defined(__linux__)
# include <unistd.h>
# include <sys/syscall.h>
# if !defined(gettid)
#  if defined(__NR_gettid)
#   define gettid() syscall(__NR_gettid)
#  endif
#  if defined(__BIONIC__) && !defined(gettid)
#   define gettid() gettid()
#  endif
#  if !defined(gettid)
#   error "Could not find gettid()"
#  endif
# endif
#endif

static char const *
log_level_name_(enum B_LogLevel log_level) {
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
        B_LOG_FILE, "[%s ", log_level_name_(log_level));

    // Current time:
    // Year-Month-Day Hour:Minute:Second.Microsecond
    {
        char buffer[256];
        struct timeval now_tv;
        if (gettimeofday(&now_tv, NULL) == 0) {
            struct tm now_tm;
            (void) localtime_r(
                &(time_t) {now_tv.tv_sec}, &now_tm);
            size_t rc = strftime(
                buffer,
                sizeof(buffer),
                "%Y-%m-%d %H:%M:%S",
                &now_tm);
            if (rc != 0 && rc < sizeof(buffer)
                    && buffer[rc] == '\0') {
                (void) fprintf(
                    B_LOG_FILE,
                    "%s.%06u ",
                    buffer,
                    (unsigned int) now_tv.tv_usec);
            }
        }
    }

    // Thread ID.
    // TODO(strager): Print thread name as well.
#if defined(__MACH__)
    (void) fprintf(B_LOG_FILE, "(%u) ", mach_thread_self());
#elif defined(__linux__)
    (void) fprintf(B_LOG_FILE, "(%ld) ", gettid());
#elif defined(B_CONFIG_PTHREAD)
    // FIXME(strager): Not portable (e.g. fails on
    // pthread-win32).
    (void) fprintf(
        B_LOG_FILE, "(%p) ", (void *) pthread_self());
#endif

    // File name, line number.
    // FIXME(strager): %zu is non-portable.
    (void) fprintf(
        B_LOG_FILE, "%s:%zu]: ", file_name, line_number);

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
b_log_lock(void) {
#if defined(B_CONFIG_PTHREAD)
    int rc = pthread_mutex_lock(&s_log_lock_);
    return rc == 0;
#else
    return false;
#endif
}

B_EXPORT void
b_log_unlock(void) {
#if defined(B_CONFIG_PTHREAD)
    int rc = pthread_mutex_unlock(&s_log_lock_);
    B_ASSERT(rc == 0);
#endif
}
