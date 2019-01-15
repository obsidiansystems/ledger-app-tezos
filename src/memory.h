#pragma once

#include <string.h>

#define COMPARE(a, b) ({ \
    _Static_assert(sizeof(*a) == sizeof(*b), "Size mismatch in COMPARE"); \
    memcmp(a, b, sizeof(*a)); \
})
#define NUM_ELEMENTS(a) (sizeof(a)/sizeof(*a))
