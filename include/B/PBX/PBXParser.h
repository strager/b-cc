#ifndef B_HEADER_GUARD_238279E0_B3FE_4C5D_8AF5_5123268599C7
#define B_HEADER_GUARD_238279E0_B3FE_4C5D_8AF5_5123268599C7

#include <B/Base.h>
#include <B/FilePath.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct B_ErrorHandler;

struct B_PBXValueRange {
    size_t start;
    size_t end;
};

// An ID for a PBX object, such as CCAD436E18E286AA00EEE595.
// Some tools always emit 24-byte IDs; others emit IDs of
// arbitrary size.
struct B_PBXObjectID {
    uint8_t const *utf8;
    size_t size;
};

struct B_PBXHeader {
    uint32_t archive_version;
    uint32_t object_version;
    struct B_PBXObjectID root_object_id;
};

// Given to b_pbx_parse and called for each object in a PBX
// file.
//
// A PBX object represented by its ID and the range of bytes
// for the value of the object in the PBX plist.  For
// example, given an incomplete PBX file:
//
//     {objects={ABCD={isa=Foobar;};};}  <-- ASCII data
//     01234567890123456789012345678901  <-- Offset mod 10
//
// calling b_pbx_parse will yield:
//
//     (struct B_PBXObjectID) { "ABCD" }
//     (struct B_PBXValueRange) {
//         .start = 15,
//         .end = 28,
//     }
//
// Note that:
//
// * start and end are 0-based,
// * The offset range points to the value, including braces
//   (or quotes, etc., as appropriate), excluding the object
//   ID, and
// * end points to one byte past the end of the value.
//
// The PBXObjectID is not owned by the callback; if the
// callback wishes to preserve a PBXObjectID, it must be
// copied before returning.
typedef B_FUNC
B_PBXObjectRangeCallback(
        struct B_PBXObjectID,
        struct B_PBXValueRange,
        void *opaque,
        struct B_ErrorHandler const *);

#if defined(__cplusplus)
extern "C" {
#endif

// Parses PBX file metadata, yielding:
//
// * Every PBX object in the file via a callback, and
// * PBX file information (e.g. the ID of the root
//   PBXProject object) via an output parameter.
//
// The PBXObjectID-s returned in the PBXHeader are owned by
// data.  They will remain valid while data remains valid.
B_EXPORT_FUNC
b_pbx_parse(
        uint8_t const *data_utf8,
        size_t data_size,
        B_OUT struct B_PBXHeader *,
        B_PBXObjectRangeCallback *callback,
        void *callback_opaque,
        struct B_ErrorHandler const *);

#if defined(__cplusplus)
}
#endif

#if defined(__cplusplus)
# include <functional>

inline B_FUNC
b_pbx_parse(
        uint8_t const *data_utf8,
        size_t data_size,
        B_OUT B_PBXHeader *header,
        std::function<B_FUNC(
                B_PBXObjectID,
                B_PBXValueRange,
                B_ErrorHandler const *)> const &callback,
        B_ErrorHandler const *eh) {
    return b_pbx_parse(
        data_utf8,
        data_size,
        header,
        [](
                B_PBXObjectID object_id,
                B_PBXValueRange value_range,
                void *opaque,
                B_ErrorHandler const *eh) -> B_FUNC {
            return (*static_cast<std::function<B_FUNC(
                    B_PBXObjectID,
                    B_PBXValueRange,
                    B_ErrorHandler const *)> const *>(
                opaque))(object_id, value_range, eh);
        },
        const_cast<void *>(
            static_cast<void const *>(&callback)),
        eh);
}
#endif

#endif
