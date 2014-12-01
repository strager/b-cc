#include <B/Assert.h>

#include <stdio.h>
#include <stdlib.h>

B_EXPORT void
b_assertion_failure(
        B_OPT char const *function,
        B_OPT char const *file,
        int64_t line,
        char const *message) {
    if (function) {
        (void) fprintf(stderr, "%s:", function);
    }
    if (file) {
        (void) fprintf(stderr, "%s:", file);
    }
    if (line >= 0) {
        (void) fprintf(
            stderr, "%llu:", (unsigned long long)line);
    }
    (void) fprintf(
        stderr, " Assertion failed: %s\n", message);
    abort();
    _Exit(1);
}
