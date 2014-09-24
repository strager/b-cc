#ifndef B_HEADER_GUARD_74620237_3A50_4F49_87C0_3ECA94843444
#define B_HEADER_GUARD_74620237_3A50_4F49_87C0_3ECA94843444

#include <B/Base.h>
#include <B/Config.h>

#include <stddef.h>

struct B_ErrorHandler;

#if defined(__cplusplus)
extern "C" {
#endif

B_EXPORT_FUNC
b_allocate(
        size_t byte_count,
        B_OUTPTR void **,
        struct B_ErrorHandler const *);

B_EXPORT_FUNC
b_deallocate(
        B_TRANSFER void *,
        struct B_ErrorHandler const *);

// Ownership is *not* transferred if this function fails.
// This matches the semantics of realloc, not reallocf.
B_EXPORT_FUNC
b_reallocate(
        size_t new_byte_count,
        B_TRANSFER void *,
        B_OUTPTR void **,
        struct B_ErrorHandler const *);

B_EXPORT_FUNC
b_memdup(
        void const *data,
        size_t byte_count,
        B_OUTPTR void **,
        struct B_ErrorHandler const *);

// Allocates memory and copies from string until a 0
// terminator is found.  Resulting data is 0-terminated.
B_EXPORT_FUNC
b_strdup(
        char const *string,
        B_OUTPTR char **,
        struct B_ErrorHandler const *);

// Allocates memory and copies from string until either a 0
// terminator is found or string_length bytes have been
// copied.  Resulting data is 0-terminated.
B_EXPORT_FUNC
b_strndup(
        char const *string,
        size_t string_length,
        B_OUTPTR char **,
        struct B_ErrorHandler const *);

#if defined(__cplusplus)
}
#endif

#if defined(__cplusplus)
# include <cstddef>
# include <memory>
# include <new>
# include <type_traits>

template<typename T>
struct B_Allocator {
    typedef std::allocator_traits<B_Allocator<T>> Traits;

    typedef T value_type;

# if defined(B_CONFIG_ALLOCATOR_TYPEDEFS_REQUIRED)
    typedef T *pointer;
    typedef T const *const_pointer;
    typedef T &reference;
    typedef T const &const_reference;
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;
# endif

# if defined(B_CONFIG_ALLOCATOR_REBIND_REQUIRED)
    template<typename TOther>
    struct rebind {
        typedef B_Allocator<TOther> other;
    };
# endif

    B_Allocator() {
    }

    B_Allocator(B_Allocator const &) {
    }

    template<class TOther>
    B_Allocator(B_Allocator<TOther> const &) {
    }

    template<class TOther>
    B_Allocator(B_Allocator<const TOther> &&) {
    }

    B_Allocator &
    operator=(B_Allocator const &) {
    }

    B_Allocator &
    operator=(B_Allocator &&) {
    }

# if defined(B_CONFIG_ALLOCATOR_TYPEDEFS_REQUIRED)
    pointer
# else
    typename Traits::pointer
# endif
    allocate(
# if defined(B_CONFIG_ALLOCATOR_TYPEDEFS_REQUIRED)
            size_type n) {
# else
            typename Traits::size_type n) {
# endif
        void *p;
        // TODO(strager): Check for overflow.
        size_t byte_count = n * sizeof(T);
        if (!b_allocate(byte_count, &p, nullptr)) {
# if defined(B_CONFIG_CXX_EXCEPTIONS)
            throw std::bad_alloc();
# else
            p = nullptr;
# endif
        }
# if defined(B_CONFIG_ALLOCATOR_TYPEDEFS_REQUIRED)
        return (pointer) p;
# else
        return (typename Traits::pointer) p;
# endif
    }

    void
    deallocate(
# if defined(B_CONFIG_ALLOCATOR_TYPEDEFS_REQUIRED)
            pointer p,
            size_type) {
# else
            typename Traits::pointer p,
            typename Traits::size_type) {
# endif
        if (!b_deallocate(p, nullptr)) {
# if defined(B_CONFIG_CXX_EXCEPTIONS)
            // FIXME(strager)
            throw std::bad_alloc();
# endif
        }
    }

# if defined(B_CONFIG_ALLOCATOR_CONSTRUCT_REQUIRED)
    template<class TOther, class ...TArgs>
    void
    construct(TOther *p, TArgs &&...args) {
        ::new((void *) p) TOther(std::forward<TArgs>(args)...);
    }

    template<class TOther>
    void
    destroy(TOther *p) {
        p->~TOther();
    }
# endif

    bool
    operator==(B_Allocator const &) {
        return true;
    }

    bool
    operator!=(B_Allocator const &other) {
        return !(*this == other);
    }
};

template<typename T, class ...TArgs>
B_FUNC
b_new(
        B_OUTPTR T **out,
        struct B_ErrorHandler const *eh,
        TArgs &&...args) {
    (void) eh;  // TODO(strager)

    B_Allocator<T> allocator;
    typedef typename B_Allocator<T>::Traits Traits;
    typename Traits::pointer p
        = Traits::allocate(allocator, 1);

    Traits::construct(
        allocator, p, std::forward<TArgs>(args)...);

    *out = p;
    return true;  // TODO(strager)
}

template<typename T, class ...TArgs>
B_FUNC
b_delete(T *p, struct B_ErrorHandler const *eh) {
    (void) eh;  // TODO(strager)

    B_Allocator<T> allocator;
    typedef typename B_Allocator<T>::Traits Traits;
    Traits::destroy(allocator, p);
    Traits::deallocate(allocator, p, 1);

    return true;  // TODO(strager)
}

#endif

#endif
