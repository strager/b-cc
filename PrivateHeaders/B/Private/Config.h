#pragma once

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
# define B_CONFIG_C_STATIC_ASSERT 1
#else
# define B_CONFIG_C_STATIC_ASSERT 0
#endif

#if defined(__APPLE__) || defined(__gnu_linux__)
# define B_CONFIG_POSIX_SPAWN 1
#else
# define B_CONFIG_POSIX_SPAWN 0
#endif

// TODO(strager)
#define B_CONFIG_POSIX_PROCESS 1
#define B_CONFIG_POSIX_SIGNALS 1

#if defined(__linux__)
// glibc's posix_spawn is pedantic about signal sets. If you
// try to force SIG_DFL for e.g. SIGKILL, the child process
// will exit with code 127.
# define B_CONFIG_PEDANTIC_POSIX_SPAWN_SIGNALS 1
#else
# define B_CONFIG_PEDANTIC_POSIX_SPAWN_SIGNALS 0
#endif

#if defined(__linux__)
// glibc's sigfillset sets too many signals, which causes
// problems with some APIs using sigset_t, especially under
// Valgrind.
# define B_CONFIG_BUGGY_SIGFILLSET 1
#else
# define B_CONFIG_BUGGY_SIGFILLSET 0
#endif

#if defined(__APPLE__) || defined(__FreeBSD__)
# define B_CONFIG_KQUEUE 1
#else
# define B_CONFIG_KQUEUE 0
#endif

#if defined(__APPLE__)
// pselect is poorly implemented on certain platforms.
# define B_CONFIG_BROKEN_PSELECT 1
#else
# define B_CONFIG_BROKEN_PSELECT 0
#endif
