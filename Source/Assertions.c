#include <B/Private/Assertions.h>

#include <string.h>

B_WUR B_EXPORT_FUNC void
b_scribble(
    B_BORROW void *data,
    size_t size) {
  memset(data, 0x5E, size);
}
