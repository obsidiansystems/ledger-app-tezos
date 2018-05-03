#include "protocol.h"

#include <stdint.h>
#include <string.h>

#include "os.h"

struct block {
    int32_t level;
    uint8_t proto;
    char predecessor[32];
    uint64_t timestamp;
};

#define SUPPORTED_PROTO_VERSION 1

int32_t N_highest_level = 0;

struct block *as_block(const uint8_t *data, size_t length) {
    if (length < sizeof(struct block)) return NULL;
    return (struct block*)data;
}

bool is_block(const uint8_t *data, size_t length) {
    const struct block *blk = as_block(data, length);
    return blk != NULL && blk->proto == SUPPORTED_PROTO_VERSION;
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

bool is_valid_block(const uint8_t *data, size_t length) {
    const struct block *blk = as_block(data, length);
    if (blk == NULL) return false;
    if (blk->proto != SUPPORTED_PROTO_VERSION) {
        return false;
    }
    int32_t level = ntohl(blk->level);
    if (level < N_highest_level) {
        return false;
    }
    // nvm_write is not declared with correct use of const
    nvm_write(&N_highest_level, (void*)&level, sizeof(N_highest_level));
    return true;
}
