#include <B/Error.h>
#include <B/RefCount.h>

#include <errno.h>

B_EXPORT_FUNC
b_ref_count_init(
        struct B_RefCount *ref_count,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, ref_count);

    ref_count->ref_count = 1;
    return true;
}

B_EXPORT_FUNC
b_ref_count_retain(
        struct B_RefCount *ref_count,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, ref_count);
    B_CHECK_PRECONDITION(eh, ref_count->ref_count != 0);

retry:
    if (ref_count->ref_count == UINT32_MAX) {
        enum B_ErrorHandlerResult result
            = B_RAISE_ERRNO_ERROR(eh, EOVERFLOW, "retain");
        switch (result) {
        case B_ERROR_ABORT:
            return false;
        case B_ERROR_RETRY:
            goto retry;
        case B_ERROR_IGNORE:
            // Assume retain succeeded.
            return true;
        }
    }

    ref_count->ref_count += 1;
    return true;
}

B_EXPORT_FUNC
b_ref_count_release(
        struct B_RefCount *ref_count,
        bool *should_dealloc,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, ref_count);
    B_CHECK_PRECONDITION(eh, ref_count->ref_count != 0);

    ref_count->ref_count -= 1;
    *should_dealloc = ref_count->ref_count == 0;
    return true;
}
