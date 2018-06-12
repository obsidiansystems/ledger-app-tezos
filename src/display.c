#include "display.h"

static inline char to_hexit(char in) {
    if (in < 0xA) {
        return in + '0';
    } else {
        return in - 0xA + 'A';
    }
}

int convert_address(char *buff, uint32_t buff_size, void *raw_bytes, uint32_t size) {
    if (size * 2 + 1 > buff_size) {
        return 0;
    }

    uint8_t *bytes = raw_bytes;
    uint32_t i;
    for (i = 0; i < size; i++) {
        buff[i * 2] = to_hexit(bytes[i] >> 4);
        buff[i * 2 + 1] = to_hexit(bytes[i] & 0xF);
    }
    buff[i * 2] = '\0';
    return i * 2 + 1;
}
