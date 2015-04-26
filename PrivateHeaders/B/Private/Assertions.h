#pragma once

#include <B/Attributes.h>
#include <B/Private/Config.h>

#include <assert.h>
#include <stddef.h>

#define B_ASSERT(cond) assert(cond)

#define B_PRECONDITION(cond) \
  do { \
    assert(cond); \
  } while (0)

// Scribble B_OUT parameters.
#define B_OUT_PARAMETER(param) \
  do { \
    B_PRECONDITION(param); \
    b_scribble((param), sizeof(*(param))); \
  } while (0)

#define B_BUG() \
  do { \
    B_ASSERT(0 && "Bug!"); \
  } while (0)

#define B_UNREACHABLE() \
  do { \
    B_BUG(); \
  } while (0)

#if B_CONFIG_C_STATIC_ASSERT
# define B_STATIC_ASSERT(_cond, _message) \
  _Static_assert(_cond, _message)
#else
# define B_STATIC_ASSERT(_cond, _message) \
  extern void b_static_assert_failure_( \
    int _[(_cond) ? 1 : -1])
#endif

B_WUR B_EXPORT_FUNC void
b_scribble(
    B_BORROW void *data,
    size_t size);
