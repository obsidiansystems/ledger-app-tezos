#include "protocol.h"

#include <stdint.h>
#include <string.h>

#include "os.h"

struct block {
    char magic_byte;
    int32_t level;
    uint8_t proto;
    // ... beyond this we don't care
};

#define SUPPORTED_PROTO_VERSION 1

int32_t N_highest_level = 0;

struct block *as_block(const uint8_t *data, size_t length) {
    if (get_magic_byte(data, length) != MAGIC_BYTE_BLOCK) return NULL;
    if (length < sizeof(struct block)) return NULL;
    return (struct block*)data;
}

bool is_block(const uint8_t *data, size_t length) {
    const struct block *blk = as_block(data, length);
    return blk != NULL;
}
