#ifndef B_HEADER_GUARD_E31A341A_CF81_49A5_A906_7D60CEC1B8B8
#define B_HEADER_GUARD_E31A341A_CF81_49A5_A906_7D60CEC1B8B8

#include <B/Base.h>

#include <stdint.h>

// A reference count.
//
// Thread-safe: NO
// Signal-safe: YES
struct B_RefCount;

struct B_RefCount {
    uint32_t ref_count;
};

#if defined(__cplusplus)
#define B_REF_COUNT_INITIALIZER (B_RefCount{1})
#else
#define B_REF_COUNT_INITIALIZER \
    ((struct B_RefCount) { \
        .ref_count = 1, \
    })
#endif

#define B_REF_COUNTED_NAME_ _object_ref_count

#define B_REF_COUNTED_OBJECT \
    struct B_RefCount B_REF_COUNTED_NAME_

#define B_REF_COUNTED_OBJECT_INIT(_object, _eh) \
    (b_ref_count_init(&(_object)->B_REF_COUNTED_NAME_, eh))

#define B_RETAIN(_object, _eh) \
    (b_ref_count_retain( \
        &(_object)->B_REF_COUNTED_NAME_, \
        eh))
#define B_RELEASE(_object, _should_dealloc, _eh) \
    (b_ref_count_release( \
        &(_object)->B_REF_COUNTED_NAME_, \
        (_should_dealloc), \
        eh))

#if defined(__cplusplus)
extern "C" {
#endif

// Initializes the RefCount with a value of (i.e. takes
// ownership of a RefCount).
B_EXPORT_FUNC
b_ref_count_init(
        struct B_RefCount *,
        struct B_ErrorHandler const *);

// Increments the RefCount.
B_EXPORT_FUNC
b_ref_count_retain(
        struct B_RefCount *,
        struct B_ErrorHandler const *);

// Decrements the RefCount.  If the ref count reaches zero,
// *should_dealloc is set to true.  Otherwise,
// *should_dealloc is set to false.
B_EXPORT_FUNC
b_ref_count_release(
        struct B_RefCount *,
        bool *should_dealloc,
        struct B_ErrorHandler const *);

#if defined(__cplusplus)
}
#endif

#endif
