#include "apdu.h"
#include "baking_auth.h"
#include "cx.h"
#include "os.h"
#include "paths.h"
#include "protocol.h"

#include <string.h>

WIDE nvram_data N_data_real;

bool is_valid_level(level_t lvl) {
    return !(lvl & 0xC0000000);
}

void write_highest_level(level_t lvl) {
    if (!is_valid_level(lvl)) return;
    nvm_write((void*)&N_data.highest_level, &lvl, sizeof(lvl));
    change_idle_display(N_data.highest_level);
}

void authorize_baking(cx_curve_t curve, uint32_t *bip32_path, uint8_t path_length) {
    if (path_length > MAX_BIP32_PATH || path_length == 0) {
        return;
    }

    nvram_data new_baking_details;
    new_baking_details.highest_level = N_data.highest_level;
    new_baking_details.curve = curve;
    memcpy(new_baking_details.bip32_path, bip32_path, path_length * sizeof(*bip32_path));
    new_baking_details.path_length = path_length;
    nvm_write((void*)&N_data, &new_baking_details, sizeof(N_data));
    change_idle_display(N_data.highest_level);
}

bool is_level_authorized(level_t level) {
    return is_valid_level(level) && level > N_data.highest_level;
}

bool is_path_authorized(cx_curve_t curve, uint32_t *bip32_path, uint8_t path_length) {
    return path_length != 0 &&
        path_length == N_data.path_length &&
        curve == N_data.curve &&
        memcmp(bip32_path, N_data.bip32_path, path_length * sizeof(*bip32_path)) == 0;
}

void guard_baking_authorized(cx_curve_t curve, void *data, int datalen, uint32_t *bip32_path,
                             uint8_t path_length) {
    if (is_block_valid(data, datalen)) {
        level_t level = get_block_level(data, datalen);
        if (!is_level_authorized(level)) {
            THROW(EXC_SECURITY);
        }
    } else if (get_magic_byte(data, datalen) == MAGIC_BYTE_BLOCK) {
        THROW(EXC_SECURITY);
    }
    if (!is_path_authorized(curve, bip32_path, path_length)) {
        THROW(EXC_SECURITY);
    }
}

void update_high_water_mark(void *data, int datalen) {
    if (is_block_valid(data, datalen)) {
        level_t level = get_block_level(data, datalen);
        if (is_valid_level(level) && level > N_data.highest_level) {
            write_highest_level(level);
        }
    }
}

void update_auth_text(void) {
    if (N_data.path_length == 0) {
        strcpy(baking_auth_text, "No Key Authorized");
    } else {
        cx_ecfp_public_key_t pub_key;
        cx_ecfp_private_key_t priv_key;
        generate_key_pair(N_data.curve, N_data.path_length, N_data.bip32_path,
                          &pub_key, &priv_key);
        os_memset(&priv_key, 0, sizeof(priv_key));
        convert_address(baking_auth_text, sizeof(baking_auth_text), N_data.curve, &pub_key);
    }
}
