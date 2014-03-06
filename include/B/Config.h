#ifndef B_HEADER_GUARD_CCBEBEE0_B7E5_4FA2_9D80_0EE41CAC399D
#define B_HEADER_GUARD_CCBEBEE0_B7E5_4FA2_9D80_0EE41CAC399D

#if defined(_MSCVER)
# define B_CONFIG_SAL
#endif

#if defined(__GNUC__) || defined(__clang__)
# define B_CONFIG_GNU_ATTRIBUTES
#endif

#if defined(__cplusplus)
// Test for libc++ bug where an allocator *must* define
// rebind for std::allocator_traits<T>::rebind to work.
# include <string>
# if defined(_LIBCPP_VERSION) && _LIBCPP_VERSION <= 1101
#  define B_CONFIG_ALLOCATOR_REBIND_REQUIRED
# endif
#endif

#if defined(__cplusplus)
// Test for possible libc++ bug (or my misunderstanding of
// how things work) where an allocator *must* define
// pointer, const_pointer, etc. to work with STL containers.
# include <string>
# if defined(_LIBCPP_VERSION) && _LIBCPP_VERSION <= 1101
#  define B_CONFIG_ALLOCATOR_TYPEDEFS_REQUIRED
# endif
#endif

#if defined(__APPLE__)
# define B_CONFIG_PTHREAD
#endif

#if defined(__APPLE__)
# define B_CONFIG_POSIX_SPAWN
#endif

#if defined(__APPLE__)
# define B_CONFIG_VFORK
#endif

#if defined(__APPLE__) || defined(__FreeBSD__)
# define B_CONFIG_KQUEUE
#endif

#endif
