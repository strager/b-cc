#include <B/Error.h>
#include <B/Private/Assertions.h>
#include <B/Private/Memory.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

B_WUR B_EXPORT_FUNC bool
b_allocate(
    size_t size,
    B_OUT_TRANSFER void **out,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(size > 0);
  B_OUT_PARAMETER(out);
  B_OUT_PARAMETER(e);

  void *m;
  if (!b_allocate2(size, 1, &m, e)) {
    return false;
  }
  *out = m;
  return true;
}

B_WUR B_EXPORT_FUNC bool
b_allocate2(
    size_t size1,
    size_t size2,
    B_OUT_TRANSFER void **out,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(size1 > 0);
  B_PRECONDITION(size2 > 0);
  B_OUT_PARAMETER(out);
  B_OUT_PARAMETER(e);

  void *m = calloc(size1, size2);
  if (!m) {
    *e = (struct B_Error) {
      .posix_error = ENOMEM,
    };
    return false;
  }
  *out = m;
  return true;
}

B_WUR B_EXPORT_FUNC bool
b_reallocate(
    B_TRANSFER void *m,
    size_t size,
    B_OUT_TRANSFER void **out,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(m);
  B_PRECONDITION(size > 0);
  B_OUT_PARAMETER(out);
  B_OUT_PARAMETER(e);

  void *new_m = realloc(m, size);
  if (!new_m) {
    *e = (struct B_Error) {.posix_error = ENOMEM};
    return false;
  }
  *out = new_m;
  return true;
}

B_WUR B_EXPORT_FUNC void
b_deallocate(
    B_TRANSFER void *memory) {
  B_PRECONDITION(memory);

  free(memory);
}

B_WUR B_EXPORT_FUNC bool
b_strdup(
    B_BORROW char const *string,
    B_OUT_TRANSFER char **out,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(string);
  B_OUT_PARAMETER(out);
  B_OUT_PARAMETER(e);

  char *m;
  if (!b_memdup(
      string, strlen(string) + 1, (void **) &m, e)) {
    return false;
  }
  *out = m;
  return true;
}

B_WUR B_EXPORT_FUNC bool
b_memdup(
    B_BORROW void const *memory,
    size_t size,
    B_OUT_TRANSFER void **out,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(memory);
  B_PRECONDITION(size > 0);
  B_OUT_PARAMETER(out);
  B_OUT_PARAMETER(e);

  void *m;
  if (!b_allocate(size, &m, e)) {
    return false;
  }
  memcpy(m, memory, size);
  *out = m;
  return true;
}
