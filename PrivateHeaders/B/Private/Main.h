#pragma once

#include <B/Attributes.h>

struct B_RunLoop;

struct B_Main;

#if defined(__cplusplus)
extern "C" {
#endif

B_WUR B_FUNC B_BORROW struct B_RunLoop *
b_main_run_loop(
    B_BORROW struct B_Main *);

// For more methods, see <B/Main.h>.

#if defined(__cplusplus)
}
#endif
