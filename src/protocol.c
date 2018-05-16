#include "protocol.h"

#include <stdint.h>
#include <string.h>

#include "os.h"

struct __attribute__((__packed__)) block {
    char magic_byte;
    int32_t level;
    uint8_t proto;
    // ... beyond this we don't care
};

#define SUPPORTED_PROTO_VERSION 1

bool is_block(const void *data, size_t length) {
    if (length < sizeof(struct block)) return false;
    if (get_magic_byte(data, length) != MAGIC_BYTE_BLOCK) return false;
    const struct block *blk = data;
    if (blk->proto != SUPPORTED_PROTO_VERSION) return false;
    return true;
}

int32_t read_unaligned_big_endian(const void *in) {
    const uint8_t *bytes = in;
    uint8_t out_bytes[4];
    int32_t res;

    out_bytes[0] = bytes[3];
    out_bytes[1] = bytes[2];
    out_bytes[2] = bytes[1];
    out_bytes[3] = bytes[0];
    memcpy(&res, out_bytes, 4);

    return res;
}

int32_t get_block_level(const void *data, size_t length) {
    const struct block *blk = data;
    return read_unaligned_big_endian(&blk->level);
}
