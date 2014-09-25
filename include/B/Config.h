#ifndef B_HEADER_GUARD_CCBEBEE0_B7E5_4FA2_9D80_0EE41CAC399D
#define B_HEADER_GUARD_CCBEBEE0_B7E5_4FA2_9D80_0EE41CAC399D

#if defined(_MSCVER)
# define B_CONFIG_SAL
#endif

#if defined(__GNUC__) || defined(__clang__)
# define B_CONFIG_GNU_ATTRIBUTES
#endif

#if defined(__cplusplus)
// Test for libc++/libstdc++ bug where an allocator *must*
// define rebind for std::allocator_traits<T>::rebind to
// work.
# include <string>
# if (defined(_LIBCPP_VERSION) && _LIBCPP_VERSION <= 1101) \
    || (defined(__GLIBCXX__) && __GLIBCXX__ <= 20140206)
#  define B_CONFIG_ALLOCATOR_REBIND_REQUIRED
# endif
#endif

#if defined(__cplusplus)
// Test for possible libc++/libstdc++ bug (or my
// misunderstanding of how things work) where an allocator
// *must* define pointer, const_pointer, etc. to work with
// STL containers.
# include <string>
# if (defined(_LIBCPP_VERSION) && _LIBCPP_VERSION <= 1101) \
    || (defined(__GLIBCXX__) && __GLIBCXX__ <= 20140206)
#  define B_CONFIG_ALLOCATOR_TYPEDEFS_REQUIRED
# endif
#endif

#if defined(__cplusplus)
// Test for libstdc++ bug where an allocator *must* define
// construct and destroy to work with STL containers.
# if defined(__GLIBCXX__) && __GLIBCXX__ <= 20140206
#  define B_CONFIG_ALLOCATOR_CONSTRUCT_REQUIRED
# endif
#endif

#if defined(__cplusplus)
// Test for libstdc++ which is missing an std::function
// constructor accepting std::allocator_arg.
# if defined(__GLIBCXX__) && __GLIBCXX__ <= 20140206
#  define B_CONFIG_NO_FUNCTION_ALLOCATOR
# endif
#endif

#if !defined(__cplusplus)
// GCC disallows assignment of struct values where any field
// in the struct is declared const.
# if defined(__GNUC__) && !defined(__clang__)
#  define B_CONFIG_NO_CONST_STRUCT_MEMBERS
# endif
#endif

#if defined(__APPLE__) || defined(__gnu_linux__)
# define B_CONFIG_PTHREAD
#endif

#if defined(__APPLE__) || defined(__gnu_linux__)
# define B_CONFIG_POSIX_SPAWN
#endif

#if defined(__APPLE__)
# define B_CONFIG_VFORK
#endif

#if defined(__APPLE__) || defined(__FreeBSD__)
# define B_CONFIG_KQUEUE
#endif

#if defined(__linux__) || defined(__android__)
# define B_CONFIG_EPOLL
#endif

#if defined(__APPLE__) || defined(__FreeBSD__) || \
    defined(__linux__) || defined(__android__)
# define B_CONFIG_SYSQUEUE
#endif

#if defined(__APPLE__)
# define B_CONFIG_LIBKERN
#endif

#if defined(__BYTE_ORDER__) && \
    defined(__ORDER_LITTLE_ENDIAN__) && \
    defined(__ORDER_BIG_ENDIAN__)
# if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#  define B_CONFIG_LITTLE_ENDIAN
# endif
# if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#  define B_CONFIG_LITTLE_ENDIAN
# endif
#endif

#if !defined(B_CONFIG_LITTLE_ENDIAN) && \
    defined(__LITTLE_ENDIAN__)
# define B_CONFIG_LITTLE_ENDIAN
#endif
#if !defined(B_CONFIG_BIG_ENDIAN) && defined(__BIG_ENDIAN__)
# define B_CONFIG_BIG_ENDIAN
#endif

#if defined(__GNUC__) || defined(__clang__)
# define B_CONFIG_BSWAP_BUILTIN
#endif

#define B_CONFIG_POSIX_FD

#endif
