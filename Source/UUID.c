#include <B/UUID.h>

#include <string.h>

B_WUR B_EXPORT_FUNC bool
b_uuid_equal(
    B_BORROW struct B_UUID const *a,
    B_BORROW struct B_UUID const *b) {
  return memcmp(a, b, sizeof(struct B_UUID)) == 0;
}
