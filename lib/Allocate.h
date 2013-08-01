#ifndef ALLOCATE_H_10102188_B0EB_4DFA_B786_96578D0B19E5
#define ALLOCATE_H_10102188_B0EB_4DFA_B786_96578D0B19E5

#include <stdlib.h>

// Allocates a structure and initializes it.  Example usage:
//
// B_ALLOCATE(struct timespec, t, {
//     .tv_sec = seconds,
//     .tv_nsec = nanoseconds,
// });
#define B_ALLOCATE(type, var, ...) \
    type *var = (type *) malloc(sizeof(type)); \
    do { *var = (type) __VA_ARGS__; } while (0)

#endif
