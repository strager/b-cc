#ifndef B_HEADER_GUARD_CCBEBEE0_B7E5_4FA2_9D80_0EE41CAC399D
#define B_HEADER_GUARD_CCBEBEE0_B7E5_4FA2_9D80_0EE41CAC399D

#if defined(_MSCVER)
# define B_CONFIG_SAL
#endif

#if defined(__GNUC__) || defined(__clang__)
# define B_CONFIG_GNU_ATTRIBUTES
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

#if defined(__APPLE__) || defined(__FreeBSD__)
# define B_CONFIG_KQUEUE
#endif

#if defined(__linux__) || defined(__android__)
# define B_CONFIG_EPOLL
#endif

#if defined(__linux__) || defined(__android__)
# define B_CONFIG_EVENTFD
#endif

#if defined(__linux__) || defined(__android__)
# define B_CONFIG_SIGNALFD
#endif

#define B_CONFIG_POSIX_PROCESS

#define B_CONFIG_POSIX_SIGNALS

#define B_CONFIG_POSIX_FD

#if !(defined(NDEBUG) && NDEBUG)
# define B_CONFIG_DEBUG
#endif

#if defined(__linux__)
// glibc's sigfillset sets too many signals, which causes
// problems with some APIs using sigset_t, especially under
// Valgrind.
# define B_CONFIG_BUGGY_SIGFILLSET
#endif

#if defined(__linux__)
// glibc's posix_spawn is pedantic about signal sets.  If
// you try to force SIG_DFL for e.g. SIGKILL, the child
// process will exit with code 127.
# define B_CONFIG_PEDANTIC_POSIX_SPAWN_SIGNALS
#endif

#if defined(__APPLE__)
// pselect is poorly implemented on certain platforms.
# define B_CONFIG_BROKEN_PSELECT
#endif

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
# define B_CONFIG_C_STATIC_ASSERT
#endif

#if defined(__cplusplus) && __cplusplus >= 201103L
# define B_CONFIG_CXX_STATIC_ASSERT
#endif

#if (defined(__GNUC__) && defined(__GNUC_MINOR__) \
        && __GNUC__ > 4 \
        || (__GNUC__ == 4 && __GNUC_MINOR__ >= 4)) \
    || defined(__clang__)
# define B_CONFIG_WARN_UNUSED_RESULT
#endif

#endif
