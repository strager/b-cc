#include <B/Internal/Portable.h>
#include <B/Internal/PortableSignal.h>
#include <B/Log.h>

#include <assert.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *
b_strdup(
    const char *s) {

    return b_memdup(s, strlen(s) + 1);
}

char *
b_memdup(
    const char *data,
    size_t size) {

    char *new_data = malloc(size);
    memcpy(new_data, data, size);
    return new_data;
}

void *
b_reallocf(
    void *buffer,
    size_t new_size) {
    void *new_buffer = realloc(buffer, new_size);
    if (!new_buffer) {
        free(buffer);
    }
    return new_buffer;
}

size_t
b_min_size(
    size_t a,
    size_t b) {
    return a < b ? a : b;
}

struct B_ThreadClosure {
    void (*thread_function)(void *user_closure);
    void *user_closure;

    struct B_Signal *started_signal;

#if defined(__APPLE__)
    const char *thread_name;
#endif
};

static void *
b_thread_entry(
    void *closure) {

    struct B_ThreadClosure thread_closure
        = *(struct B_ThreadClosure *) closure;

#if defined(__APPLE__)
    int rc = pthread_setname_np(thread_closure.thread_name);
    assert(rc == 0);
#endif

    // Tell the main thread we've started.
    struct B_Exception *ex
        = b_signal_raise(thread_closure.started_signal);
    if (ex) {
        B_LOG_EXCEPTION(ex);
        abort();
    }

    thread_closure.thread_function(
        thread_closure.user_closure);
    return NULL;
}

B_ERRFUNC
b_create_thread(
    const char *thread_name,
    void (*thread_function)(void *user_closure),
    void *user_closure) {

    int rc;

    struct B_Signal started_signal;
    struct B_Exception *ex
        = b_signal_init(&started_signal);
    if (ex) {
        return ex;
    }

    struct B_ThreadClosure thread_closure = {
        .thread_function = thread_function,
        .user_closure = user_closure,

        .started_signal = &started_signal,

#if defined(__APPLE__)
        .thread_name = thread_name,
#endif
    };

    pthread_t thread_id;
    rc = pthread_create(
        &thread_id,
        NULL,
        b_thread_entry,
        &thread_closure);
    assert(rc == 0);

    // Wait for the thread to start.
    ex = b_signal_await(&started_signal);
    if (ex) {
        (void) b_signal_deinit(&started_signal);
        return ex;
    }

#if !defined(__APPLE__)
#warning Unsupported platform
    (void) thread_name;
#endif

    (void) b_signal_deinit(&started_signal);
    return NULL;
}

bool
b_current_thread_string(
    char *buffer,
    size_t buffer_size) {

#if defined(__APPLE__)
    int rc;

    pthread_t current_thread = pthread_self();

    char thread_name[64];
    rc = pthread_getname_np(
        current_thread,
        thread_name,
        sizeof(thread_name));
    // FIXME(strager): pthread_getname_np gladly truncates
    // the thread_name for us, returning 0.  We should
    // detect and handle this case.
    assert(rc == 0);

    // Give the main thread a nice name if none has been
    // assigned.
    if (thread_name[0] == '\0' && pthread_main_np()) {
        static char const main_thread_name[] = "(main)";
        _Static_assert(
            sizeof(thread_name) >= sizeof(main_thread_name),
            "main_thread_name should fit in thread_name");
        memcpy(
            thread_name,
            main_thread_name,
            sizeof(main_thread_name));
    }

    _Static_assert(
        sizeof(pthread_t) == sizeof(void *),
        "pthread_t must be a pointer type");
    rc = snprintf(
        buffer,
        buffer_size,
        "%p/%s",
        current_thread,
        thread_name);
    assert(rc >= 0);
    return rc == strlen(buffer);

#else
#error Unsupported platform
#endif
}
