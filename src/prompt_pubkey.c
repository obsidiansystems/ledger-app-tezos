#include "prompt_pubkey.h"

#include "apdu.h"
#include "base58.h"
#include "blake2.h"
#include "paths.h"

#include "os.h"
#include "cx.h"
#include "ui.h"

#include <string.h>

char address_display_data[64]; // Should be more than big enough

int convert_address(char *buff, uint32_t buff_size, cx_curve_t curve,
                           const cx_ecfp_public_key_t *public_key);

const bagl_element_t ui_pubkey_prompt[] = {
    // type                               userid    x    y   w    h  str rad
    // fill      fg        bg      fid iid  txt   touchparams...       ]
    {{BAGL_RECTANGLE, 0x00, 0, 0, 128, 32, 0, 0, BAGL_FILL, 0x000000, 0xFFFFFF,
      0, 0},
     NULL,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

    {{BAGL_ICON, 0x00, 3, 12, 7, 7, 0, 0, 0, 0xFFFFFF, 0x000000, 0,
      BAGL_GLYPH_ICON_CROSS},
     NULL,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
    {{BAGL_ICON, 0x00, 117, 13, 8, 6, 0, 0, 0, 0xFFFFFF, 0x000000, 0,
      BAGL_GLYPH_ICON_CHECK},
     NULL,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

    //{{BAGL_ICON                           , 0x01,  21,   9,  14,  14, 0, 0, 0
    //, 0xFFFFFF, 0x000000, 0, BAGL_GLYPH_ICON_TRANSACTION_BADGE  }, NULL, 0, 0,
    //0, NULL, NULL, NULL },
    {{BAGL_LABELINE, 0x01, 0, 12, 128, 12, 0, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_EXTRABOLD_11px | BAGL_FONT_ALIGNMENT_CENTER, 0},
     "Provide",
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
    {{BAGL_LABELINE, 0x01, 0, 26, 128, 12, 0, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_EXTRABOLD_11px | BAGL_FONT_ALIGNMENT_CENTER, 0},
     "public key?",
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

    {{BAGL_LABELINE, 0x02, 0, 12, 128, 12, 0, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_REGULAR_11px | BAGL_FONT_ALIGNMENT_CENTER, 0},
     "Public Key",
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

    {{BAGL_LABELINE, 0x02, 23, 26, 82, 12, 0x80 | 10, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_EXTRABOLD_11px | BAGL_FONT_ALIGNMENT_CENTER, 26},
     address_display_data,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
};

#ifdef BAKING_APP
const bagl_element_t ui_bake_prompt[] = {
    // type                               userid    x    y   w    h  str rad
    // fill      fg        bg      fid iid  txt   touchparams...       ]
    {{BAGL_RECTANGLE, 0x00, 0, 0, 128, 32, 0, 0, BAGL_FILL, 0x000000, 0xFFFFFF,
      0, 0},
     NULL,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

    {{BAGL_ICON, 0x00, 3, 12, 7, 7, 0, 0, 0, 0xFFFFFF, 0x000000, 0,
      BAGL_GLYPH_ICON_CROSS},
     NULL,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
    {{BAGL_ICON, 0x00, 117, 13, 8, 6, 0, 0, 0, 0xFFFFFF, 0x000000, 0,
      BAGL_GLYPH_ICON_CHECK},
     NULL,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

    //{{BAGL_ICON                           , 0x01,  21,   9,  14,  14, 0, 0, 0
    //, 0xFFFFFF, 0x000000, 0, BAGL_GLYPH_ICON_TRANSACTION_BADGE  }, NULL, 0, 0,
    //0, NULL, NULL, NULL },
    {{BAGL_LABELINE, 0x01, 0, 12, 128, 12, 0, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_EXTRABOLD_11px | BAGL_FONT_ALIGNMENT_CENTER, 0},
     "Authorize baking",
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
    {{BAGL_LABELINE, 0x01, 0, 26, 128, 12, 0, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_EXTRABOLD_11px | BAGL_FONT_ALIGNMENT_CENTER, 0},
     "with public key?",
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

    {{BAGL_LABELINE, 0x02, 0, 12, 128, 12, 0, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_REGULAR_11px | BAGL_FONT_ALIGNMENT_CENTER, 0},
     "Public Key",
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

    {{BAGL_LABELINE, 0x02, 23, 26, 82, 12, 0x80 | 10, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_EXTRABOLD_11px | BAGL_FONT_ALIGNMENT_CENTER, 26},
     address_display_data,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
};
#endif

int convert_address(char *buff, uint32_t buff_size, cx_curve_t curve,
                    const cx_ecfp_public_key_t *public_key) {
    // Data to encode
    struct __attribute__((packed)) {
        char prefix[3];
        uint8_t hash[HASH_SIZE];
        char checksum[4];
    } data;

    // prefix
    data.prefix[0] = 6;
    data.prefix[1] = 161;
    switch (curve) {
        case CX_CURVE_Ed25519: // Ed25519
            data.prefix[2] = 159;
            break;
        case CX_CURVE_SECP256K1: // Secp256k1
            data.prefix[2] = 161;
            break;
        case CX_CURVE_SECP256R1: // Secp256r1
            data.prefix[2] = 164;
            break;
        default:
            THROW(EXC_WRONG_PARAM); // Should not reach
    }

    // hash
    public_key_hash(data.hash, curve, public_key, NULL);

    // checksum -- twice because them's the rules
    uint8_t checksum[32];
    cx_hash_sha256((void*)&data, sizeof(data) - sizeof(data.checksum), checksum, sizeof(checksum));
    cx_hash_sha256(checksum, sizeof(checksum), checksum, sizeof(checksum));
    memcpy(data.checksum, checksum, sizeof(data.checksum));

    b58enc(buff, &buff_size, &data, sizeof(data));
    return buff_size;
}


void prompt_address(bool bake, cx_curve_t curve,
                    const cx_ecfp_public_key_t *key, callback_t ok_cb, callback_t cxl_cb) {
    if (!convert_address(address_display_data, sizeof(address_display_data),
                         curve, key)) {
        THROW(EXC_WRONG_VALUES);
    }

    ux_step_count = 2;
#ifdef BAKING_APP
    if (bake) {
        ui_prompt(ui_bake_prompt, sizeof(ui_bake_prompt)/sizeof(*ui_bake_prompt),
                  ok_cb, cxl_cb, two_screens_scroll_second_prepro);
    } else {
#endif
        ui_prompt(ui_pubkey_prompt, sizeof(ui_pubkey_prompt)/sizeof(*ui_pubkey_prompt),
                  ok_cb, cxl_cb, two_screens_scroll_second_prepro);
#ifdef BAKING_APP
    }
#endif
}
