#pragma once

#include <stdint.h>

// Force maximum alignment.
// TODO(strager): Vector types (SSE, NEON)?
// TODO(strager): Float types (long double, complex)?
union B_UserData {
  uint8_t bytes[1];
  char char_padding;
  long long_padding;
  long long long_long_padding;
  void *pointer_padding;
  void (*function_padding)(void);
};

