#ifndef B_HEADER_GUARD_5EA06BDC_8DD2_4599_9140_AFD5E35EC653
#define B_HEADER_GUARD_5EA06BDC_8DD2_4599_9140_AFD5E35EC653

#include <B/Base.h>

struct B_ErrorHandler;

// A pointer to a FilePath is a 0-terminated string
// representing a file system path, e.g. "C:\\foo.txt" or
// "/home/me/foo".
//
// Depending upon the operating system and underlying file
// system, the encoding of a FilePath may be binary, ASCII,
// UTF-8, NFC-normalized UTF-8, or some other encoding.
//
// On UNIX platforms, a FilePath is treated as binary and
// sent directly to system calls such as open().  Zero bytes
// are disallowed.
//
// On Windows, a FilePath is treated as UTF-8 and converted
// to Unicode (UTF-16) before calling system calls such as
// CreateFileW and NtCreateFile.  Zero bytes are disallowed.
typedef char B_FilePath;

#define B_CHECK_FILE_PATH(_path, _eh) \
    do { \
        if (!b_file_path_validate((_path), (_eh))) { \
            return false; \
        } \
    } while (0)

#if defined(__cplusplus)
extern "C" {
#endif

B_EXPORT_FUNC
b_file_path_validate(
        B_FilePath const *,
        struct B_ErrorHandler const *);

B_EXPORT_FUNC
b_file_path_equal(
        B_FilePath const *,
        B_FilePath const *,
        B_OUT bool *,
        struct B_ErrorHandler const *);

#if defined(__cplusplus)
}
#endif

#endif
