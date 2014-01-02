#ifndef PORTABLESIGNAL_H_17E1DFDB_0D07_460A_B18F_CFFA4E371508
#define PORTABLESIGNAL_H_17E1DFDB_0D07_460A_B18F_CFFA4E371508

#include <B/Internal/Common.h>

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct B_Signal;

#if defined(B_UCONTEXT_IMPL)
struct B_SignalOpaque {
#else
struct B_Signal {
#endif
#if defined(__APPLE__) && defined(__x86_64__)
    uint8_t bytes[120];
#else
#error Unsupported platform/architecture
#endif
};

B_ERRFUNC
b_signal_init(
    struct B_Signal *);

B_ERRFUNC
b_signal_deinit(
    struct B_Signal *);

B_ERRFUNC
b_signal_await(
    struct B_Signal *);

B_ERRFUNC
b_signal_raise(
    struct B_Signal *);

#ifdef __cplusplus
}
#endif

#endif
