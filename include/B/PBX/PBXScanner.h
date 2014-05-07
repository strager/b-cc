#ifndef B_HEADER_GUARD_5558AB46_03AD_4C96_8024_578CFA9E03F4
#define B_HEADER_GUARD_5558AB46_03AD_4C96_8024_578CFA9E03F4

#include <B/Base.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct B_ErrorHandler;

struct B_PBXScanner;

// Given:
//
//     {a=(b,c),d=e}
//
// and assuming *recurse is set to true always, the visitor
// callbacks called will be:
//
// 1. visit_dict_begin
// 2. visit_dict_key("a")
// 3. visit_array_begin
// 4. visit_string("b")
// 5. visit_string("c")
// 6. visit_array_end
// 7. visit_dict_key("d")
// 8. visit_string("e")
// 9. visit_dict_end
//
// For dictionaries, visit_dict_key is called the first
// entry's key, followed by visitor(s) for the first entry's
// value, followed by visit_dict_key for the second entry's
// key, etc.
//
// If *recurse is set to false, the child values are never
// visited, but visit_dict_end or visit_array_end *is*
// called.
struct B_PBXValueVisitor {
    B_FUNC
    (*visit_string)(
            struct B_PBXValueVisitor *,
            struct B_PBXScanner *,
            uint8_t const *data_utf8,
            size_t data_size,
            struct B_ErrorHandler const *);

    B_FUNC
    (*visit_array_begin)(
            struct B_PBXValueVisitor *,
            struct B_PBXScanner *,
            B_OUT bool *recurse,
            struct B_ErrorHandler const *);

    B_FUNC
    (*visit_array_end)(
            struct B_PBXValueVisitor *,
            struct B_PBXScanner *,
            struct B_ErrorHandler const *);

    B_FUNC
    (*visit_dict_begin)(
            struct B_PBXValueVisitor *,
            struct B_PBXScanner *,
            B_OUT bool *recurse,
            struct B_ErrorHandler const *);

    B_FUNC
    (*visit_dict_key)(
            struct B_PBXValueVisitor *,
            struct B_PBXScanner *,
            uint8_t const *data_utf8,
            size_t data_size,
            struct B_ErrorHandler const *);

    B_FUNC
    (*visit_dict_end)(
            struct B_PBXValueVisitor *,
            struct B_PBXScanner *,
            struct B_ErrorHandler const *);
};

#if defined(__cplusplus)
extern "C" {
#endif

// Parses a PBX value from memory, calling functions of the
// given PBXValueVisitor.
B_EXPORT_FUNC
b_pbx_scan_value(
        uint8_t const *data,
        size_t data_size,
        struct B_PBXValueVisitor *,
        struct B_ErrorHandler const *);

B_EXPORT_FUNC
b_pbx_scanner_offset(
        struct B_PBXScanner *,
        B_OUT size_t *,
        struct B_ErrorHandler const *);

#if defined(__cplusplus)
}
#endif

#if defined(__cplusplus)
# include <B/Assert.h>

template<typename T>
class B_PBXValueVisitorClass :
        public B_PBXValueVisitor {
public:
    B_PBXValueVisitorClass() :
            B_PBXValueVisitor{
                visit_string_,
                visit_array_begin_,
                visit_array_end_,
                visit_dict_begin_,
                visit_dict_key_,
                visit_dict_end_,
            } {
    }

    B_FUNC
    visit_string(
            B_PBXScanner *,
            uint8_t const *data_utf8,
            size_t data_size,
            B_ErrorHandler const *) {
        (void) data_utf8;
        (void) data_size;
        B_ASSERT_MUST_OVERRIDE(T, visit_string);
    }

    B_FUNC
    visit_array_begin(
            B_PBXScanner *,
            B_OUT bool *recurse,
            B_ErrorHandler const *) {
        (void) recurse;
        B_ASSERT_MUST_OVERRIDE(T, visit_array_begin);
    }

    B_FUNC
    visit_array_end(
            B_PBXScanner *,
            B_ErrorHandler const *) {
        B_ASSERT_MUST_OVERRIDE(T, visit_array_end);
    }

    B_FUNC
    visit_dict_begin(
            B_PBXScanner *,
            B_OUT bool *recurse,
            B_ErrorHandler const *) {
        (void) recurse;
        B_ASSERT_MUST_OVERRIDE(T, visit_dict_begin);
    }

    B_FUNC
    visit_dict_key(
            B_PBXScanner *,
            uint8_t const *data_utf8,
            size_t data_size,
            B_ErrorHandler const *) {
        (void) data_utf8;
        (void) data_size;
        B_ASSERT_MUST_OVERRIDE(T, visit_dict_key);
    }

    B_FUNC
    visit_dict_end(
            B_PBXScanner *,
            B_ErrorHandler const *) {
        B_ASSERT_MUST_OVERRIDE(T, visit_dict_end);
    }

private:
    static B_FUNC
    visit_string_(
            B_PBXValueVisitor *visitor,
            B_PBXScanner *scanner,
            uint8_t const *data_utf8,
            size_t data_size,
            B_ErrorHandler const *eh) {
        return static_cast<T *>(visitor)->visit_string(
            scanner,
            data_utf8,
            data_size,
            eh);
    }

    static B_FUNC
    visit_array_begin_(
            B_PBXValueVisitor *visitor,
            B_PBXScanner *scanner,
            B_OUT bool *recurse,
            B_ErrorHandler const *eh) {
        return static_cast<T *>(visitor)
            ->visit_array_begin(scanner, recurse, eh);
    }

    static B_FUNC
    visit_array_end_(
            B_PBXValueVisitor *visitor,
            B_PBXScanner *scanner,
            B_ErrorHandler const *eh) {
        return static_cast<T *>(visitor)
            ->visit_array_end(scanner, eh);
    }

    static B_FUNC
    visit_dict_begin_(
            B_PBXValueVisitor *visitor,
            B_PBXScanner *scanner,
            B_OUT bool *recurse,
            B_ErrorHandler const *eh) {
        return static_cast<T *>(visitor)
            ->visit_dict_begin(scanner, recurse, eh);
    }

    static B_FUNC
    visit_dict_key_(
            B_PBXValueVisitor *visitor,
            B_PBXScanner *scanner,
            uint8_t const *data_utf8,
            size_t data_size,
            B_ErrorHandler const *eh) {
        return static_cast<T *>(visitor)->visit_dict_key(
            scanner,
            data_utf8,
            data_size,
            eh);
    }

    static B_FUNC
    visit_dict_end_(
            B_PBXValueVisitor *visitor,
            B_PBXScanner *scanner,
            B_ErrorHandler const *eh) {
        return static_cast<T *>(visitor)
            ->visit_dict_end(scanner, eh);
    }
};
#endif

#endif
