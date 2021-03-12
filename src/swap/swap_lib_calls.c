#include "swap_lib_calls.h"

bool swap_str_to_u64(const uint8_t* src, size_t length, uint64_t* result) {
    if (length > sizeof(uint64_t)) {
        return false;
    }
    uint64_t value = 0;
    for (size_t i = 0; i < length; i++) {
        value <<= 8;
        value |= src[i];
    }
    *result = value;
    return true;
}
