#ifndef B_HEADER_GUARD_ACA73B31_242F_4688_9059_6F739CF8D33B
#define B_HEADER_GUARD_ACA73B31_242F_4688_9059_6F739CF8D33B

#include <B/Base.h>
#include <B/FilePath.h>
#include <B/PBX/PBXParser.h>

struct B_Answer;
struct B_ErrorHandler;
struct B_PBXHeader;
struct B_QuestionVTable;

#if defined(__cplusplus)
extern "C" {
#endif

// QuestionVTable for asking about a PBX project file.  The
// given B_Question must be of type 'B_FilePath const *',
// representing a path to the file.
//
// If the Question refers to a symbolic link, the link is
// followed.  No dependency is expressed on the symbolic
// link.
struct B_QuestionVTable const *
b_pbx_project_file_question_vtable(
        void);

// Returns PBX project file metadata and objects as if by a
// call to b_pbx_parse.  The given Answer must be derived
// from a call to
// b_pbx_project_file_question_vtable()->answer.
B_EXPORT_FUNC
b_pbx_project_file_answer(
        struct B_Answer const *,
        B_OUT struct B_PBXHeader *,
        B_PBXObjectRangeCallback *callback,
        void *callback_opaque,
        struct B_ErrorHandler const *);

#if defined(__cplusplus)
}
#endif

#if defined(__cplusplus)
# include <functional>

// See also b_pbx_parse.
inline B_FUNC
b_pbx_project_file_answer(
        struct B_Answer const *answer,
        B_OUT B_PBXHeader *header,
        std::function<B_FUNC(
                B_PBXObjectID,
                B_PBXValueRange,
                B_ErrorHandler const *)> const &callback,
        B_ErrorHandler const *eh) {
    return b_pbx_project_file_answer(
        answer,
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
