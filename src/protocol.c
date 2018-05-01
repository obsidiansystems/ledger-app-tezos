#include "protocol.h"

#include <stdint.h>
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

bool is_valid_block(const uint8_t *data, size_t length) {
    const struct block *blk = as_block(data, length);
    if (blk == NULL) return false;
    if (blk->proto != SUPPORTED_PROTO_VERSION) {
        return false;
    }
    if (blk->level < N_highest_level) {
        return false;
    }
    // nvm_write is not declared with correct use of const
    nvm_write(&N_highest_level, (void*)&blk->level, sizeof(N_highest_level));
    return true;
}
