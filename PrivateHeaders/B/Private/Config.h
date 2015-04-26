#pragma once

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
# define B_CONFIG_C_STATIC_ASSERT 1
#else
# define B_CONFIG_C_STATIC_ASSERT 0
#endif
