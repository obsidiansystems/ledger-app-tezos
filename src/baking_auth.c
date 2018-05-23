#include "os.h"
#include "cx.h"
#include "baking_auth.h"
#include "paths.h"
#include "protocol.h"

#include <string.h>

WIDE nvram_data N_data_real;

void write_highest_level(int lvl) {
    nvm_write((void*)&N_data.highest_level, &lvl, sizeof(lvl));
}

bool authorize_baking(void *data, int datalen, uint32_t *bip32_path, uint8_t path_length) {
    if (path_length > MAX_BIP32_PATH || path_length == 0) {
        return false;
    }

    int level = N_data.highest_level;
    if (is_block_valid(data, datalen)) {
        level = get_block_level(data, datalen);
    }

    nvram_data new_baking_details;
    new_baking_details.highest_level = level;
    memcpy(new_baking_details.bip32_path, bip32_path, path_length * sizeof(*bip32_path));
    new_baking_details.path_length = path_length;
    nvm_write((void*)&N_data, &new_baking_details, sizeof(N_data));
    return true;
}

bool is_level_authorized(int level) {
    return level > N_data.highest_level;
}

bool is_baking_authorized(void *data, int datalen, uint32_t *bip32_path, uint8_t path_length) {
    if (is_block_valid(data, datalen)) {
        int level = get_block_level(data, datalen);
        if (!is_level_authorized(level)) {
            THROW(0x6C00);
        }
    } else if (get_magic_byte(data, datalen) == MAGIC_BYTE_BLOCK) {
        THROW(0x6C00);
    }
    return path_length != 0 &&
        path_length == N_data.path_length &&
        memcmp(bip32_path, N_data.bip32_path, path_length * sizeof(*bip32_path)) == 0;
}

void update_high_water_mark(void *data, int datalen) {
    if (is_block_valid(data, datalen)) {
        int level = get_block_level(data, datalen);
        if (level > N_data.highest_level) {
            write_highest_level(level);
        }
    }
}
