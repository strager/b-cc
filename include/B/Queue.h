#ifndef B_HEADER_GUARD_331FA090_94CC_44F9_9064_17F6C126CB3F
#define B_HEADER_GUARD_331FA090_94CC_44F9_9064_17F6C126CB3F

// FIXME(strager): We should use FreeBSD's excellent
// <sys/queue.h> implementation.  Linux does not have
// LIST_FOREACH or LIST_FOREACH_SAFE, for example.
#include <sys/queue.h>

#if !defined(LIST_FOREACH_SAFE)
# if !defined(__GLIBC__)
#  error "Missing LIST_FOREACH_SAFE"
# endif
# define LIST_FOREACH_SAFE(_var, _head, _name, _tmp_var) \
    for ( \
            (_var) = (_head)->lh_first; \
            (_var) && (((_tmp_var) = (_var)->_name.le_next), true); \
            (_var) = (_tmp_var))
#endif

#if !defined(LIST_FOREACH)
# if !defined(__GLIBC__)
#  error "Missing LIST_FOREACH"
# endif
# define LIST_FOREACH(_var, _head, _name) \
    for ( \
            (_var) = (_head)->lh_first; \
            (_var); \
            (_var) = (_var)->_name.le_next)
#endif

#if !defined(SLIST_FOREACH_SAFE)
# if !defined(__GLIBC__)
#  error "Missing SLIST_FOREACH_SAFE"
# endif
# define SLIST_FOREACH_SAFE(_var, _head, _name, _tmp_var) \
    for ( \
            (_var) = (_head)->slh_first; \
            (_var) && (((_tmp_var) = (_var)->_name.sle_next), true); \
            (_var) = (_tmp_var))
#endif

#if !defined(SLIST_FOREACH)
# if !defined(__GLIBC__)
#  error "Missing SLIST_FOREACH"
# endif
# define SLIST_FOREACH(_var, _head, _name) \
    for ( \
            (_var) = (_head)->slh_first; \
            (_var); \
            (_var) = (_var)->_name.sle_next)
#endif

#endif
