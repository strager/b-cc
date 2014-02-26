#ifndef B_HEADER_GUARD_00C8D336_BC3E_4737_B3D1_C955A0D22C39
#define B_HEADER_GUARD_00C8D336_BC3E_4737_B3D1_C955A0D22C39

#include <B/Base.h>

struct B_ErrorHandler;
struct B_Question;
struct B_QuestionVTable;

// An object-oriented interface which receives notifications
// when dependencies need to be expressed between two
// Question-s.
//
// Thread-safe: YES
// Signal-safe: NO
struct B_DependencyDelegateObject {
    B_FUNC
    (*dependency)(
            struct B_DependencyDelegateObject *,  // this
            struct B_Question const *from,
            struct B_QuestionVTable const *from_vtable,
            struct B_Question const *to,
            struct B_QuestionVTable const *to_vtable,
            struct B_ErrorHandler const *);
};

#endif
