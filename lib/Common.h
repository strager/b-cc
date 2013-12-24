#ifndef COMMON_H_90B31C3A_6F54_4F39_A359_67A70B8C419E
#define COMMON_H_90B31C3A_6F54_4F39_A359_67A70B8C419E

struct B_Exception;

#if defined(__GNUC__)
#define B_MUST_USE_RESULT __attribute__((warn_unused_result))
#else
#define B_MUST_USE_RESULT
#endif

#define B_ERRFUNC \
    B_MUST_USE_RESULT struct B_Exception *

#endif
