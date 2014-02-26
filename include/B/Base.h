#ifndef B_HEADER_GUARD_A8B25E65_8D2B_4864_A9DD_2CF119C5C6B8
#define B_HEADER_GUARD_A8B25E65_8D2B_4864_A9DD_2CF119C5C6B8

#include <B/Config.h>

#if defined(B_CONFIG_SAL)
# include <sal.h>
#endif

#include <stdbool.h>

// All functions in B are in the form:
//
// bool f(..., struct B_ErrorHandler const *eh);
//
// f will return false when an error occurs and eh->f()
// returns B_ERROR_ABORT.  B_FUNC is an alias for bool as a
// function return type.
#define B_FUNC bool

#define B_EXPORT

#define B_EXPORT_FUNC B_EXPORT B_FUNC

// SAL-like annotation.  A variable can be NULL.  B_OPT
// preceeds the variable's type:
//
// B_OPT char const *s;
#if defined(B_CONFIG_SAL)
# define B_OPT _In_opt_
#else
# define B_OPT
#endif

// SAL-like annotation.  The ownership of an argument is
// transferred when calling a function.  B_TRANSFER preceeds
// the variable's type:
//
// void free(B_TRANSFER void *);
//
// By default, pointer arguments are borrowed, excepting
// pointees to output arguments (see B_OUTPTR).
#define B_TRANSFER

// SAL-like annotation.  The containing struct or union
// borrows (does not own) the value of a field.  B_BORROWED
// preceeds the variable's type:
//
// struct Error { B_BORROWED char const *message; };
//
// By default, non-function pointers fields are owning.
#define B_BORROWED

#define B_ABSTRACT

#if defined(B_CONFIG_SAL)
# define B_OUT _Out_
# define B_OUTPTR _Outptr_
#else
# define B_OUT
# define B_OUTPTR
#endif

#endif
