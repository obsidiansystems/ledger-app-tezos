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

struct block *as_block(const uint8_t *data, size_t length) {
    if (get_magic_byte(data, length) != MAGIC_BYTE_BLOCK) return NULL;
    if (length < sizeof(struct block)) return NULL;
    struct block *res = (struct block*)data;
    if (res->proto != SUPPORTED_PROTO_VERSION) return NULL;
    return res;
}

static int32_t ntohl(int32_t in) {
    char bytes[4];
    char out_bytes[4];
    int32_t res;

    memcpy(bytes, &in, 4);
    out_bytes[0] = bytes[3];
    out_bytes[1] = bytes[2];
    out_bytes[2] = bytes[1];
    out_bytes[3] = bytes[0];
    memcpy(&res, out_bytes, 4);

    return res;
}

int32_t get_block_level(const uint8_t *data, size_t length) {
    const struct block *blk = as_block(data, length);
    if (blk == NULL) return 0;
    int32_t level;
    memcpy(&level, &blk->level, 4);
    return ntohl(level);
}
