#include <B/Error.h>
#include <B/FilePath.h>

#include <string.h>

B_EXPORT_FUNC
b_file_path_validate(
        B_FilePath const *path,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, path);
    return true;
}

B_EXPORT_FUNC
b_file_path_equal(
        B_FilePath const *a,
        B_FilePath const *b,
        B_OUT bool *out,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, a);
    B_CHECK_PRECONDITION(eh, b);
    B_CHECK_PRECONDITION(eh, out);

    if (!b_file_path_validate(a, eh)) return false;
    if (!b_file_path_validate(b, eh)) return false;

    *out = strcmp(a, b) == 0;
    return true;
}
