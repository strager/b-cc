#define B_UCONTEXT_IMPL

#include <B/Internal/PortableSignal.h>

#include <assert.h>
#include <pthread.h>
#include <stddef.h>

struct B_Signal {
    pthread_cond_t cond;
    pthread_mutex_t mutex;
    size_t signal;
};

B_ERRFUNC
b_signal_init(
    struct B_Signal *signal) {

    int rc;

    rc = pthread_cond_init(&signal->cond, NULL);
    assert(rc == 0);
    rc = pthread_mutex_init(&signal->mutex, NULL);
    assert(rc == 0);

    signal->signal = 0;

    return NULL;
}

B_ERRFUNC
b_signal_deinit(
    struct B_Signal *signal) {

    int rc;

    rc = pthread_cond_destroy(&signal->cond);
    assert(rc == 0);
    rc = pthread_mutex_destroy(&signal->mutex);
    assert(rc == 0);

    return NULL;
}

B_ERRFUNC
b_signal_await(
    struct B_Signal *signal) {

    int rc;

    rc = pthread_mutex_lock(&signal->mutex);
    assert(rc == 0);

    while (signal->signal == 0) {
        rc = pthread_cond_wait(
            &signal->cond,
            &signal->mutex);
        assert(rc == 0);
    }
    signal->signal -= 1;

    rc = pthread_mutex_unlock(&signal->mutex);
    assert(rc == 0);

    return NULL;
}

B_ERRFUNC
b_signal_raise(
    struct B_Signal *signal) {

    int rc;

    rc = pthread_mutex_lock(&signal->mutex);
    assert(rc == 0);

    signal->signal += 1;

    rc = pthread_mutex_unlock(&signal->mutex);
    assert(rc == 0);

    rc = pthread_cond_signal(&signal->cond);
    assert(rc == 0);

    return NULL;
}

_Static_assert(
    sizeof(struct B_Signal)
    == sizeof(struct B_SignalOpaque),
    "B_UContextOpaque must match B_UContext");
