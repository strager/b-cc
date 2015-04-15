#pragma once

#define B_NYI() \
  do { \
    __builtin_trap(); \
  } while (0);
