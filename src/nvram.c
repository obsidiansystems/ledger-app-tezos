#include "os.h"
#include "cx.h"
#include "nvram.h"

#include <string.h>

WIDE nvram_data N_data_real;

void write_highest_level(int lvl) {
    nvm_write((void*)&N_data.highest_level, &lvl, sizeof(lvl));
}

bool authorize_baking(int level, uint32_t *bip32_path, uint8_t path_length) {
    if (path_length > MAX_BIP32_PATH || path_length == 0) {
        return false;
    }

    nvram_data new_baking_details;
    new_baking_details.highest_level = level;
    memcpy(new_baking_details.bip32_path, bip32_path, path_length * sizeof(*bip32_path));
    new_baking_details.path_length = path_length;
    nvm_write((void*)&N_data, &new_baking_details, sizeof(N_data));
    return true;
}

bool is_baking_authorized(int level, uint32_t *bip32_path, uint8_t path_length) {
    return is_baking_authorized_general(bip32_path, path_length) &&
        level > N_data.highest_level;
}

bool is_baking_authorized_general(uint32_t *bip32_path, uint8_t path_length) {
    return path_length != 0 &&
        path_length == N_data.path_length &&
        memcmp(bip32_path, N_data.bip32_path, path_length * sizeof(*bip32_path)) == 0;
}
