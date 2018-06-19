#include "apdu_pubkey.h"

#include "apdu.h"
#include "baking_auth.h"
#include "display.h"

#include "cx.h"
#include "ui.h"

static cx_ecfp_public_key_t public_key;
static cx_curve_t curve;

// The following need to be persisted for baking app
static uint8_t path_length;
static uint32_t bip32_path[MAX_BIP32_PATH];

static const struct bagl_element_e *prompt_address_prepro(const struct bagl_element_e *element);
static void prompt_address(const bagl_element_t *prompt, size_t prompt_size,
                           void *raw_bytes, uint32_t size, callback_t ok_cb, callback_t cxl_cb);

char address_display_data[64]; // Should be more than big enough

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
static int provide_pubkey() {
    int tx = 0;
    G_io_apdu_buffer[tx++] = public_key.W_len;
    os_memmove(G_io_apdu_buffer + tx,
               public_key.W,
               public_key.W_len);
    tx += public_key.W_len;
    G_io_apdu_buffer[tx++] = 0x90;
    G_io_apdu_buffer[tx++] = 0x00;
    return tx;
}

static void pubkey_ok() {
    int tx = provide_pubkey();
    delay_send(tx);
}

#ifdef BAKING_APP
static void baking_ok() {
    authorize_baking(curve, bip32_path, path_length);
    pubkey_ok();
}
#endif

unsigned int handle_apdu_get_public_key(uint8_t instruction) {
    uint8_t privateKeyData[32];
    uint8_t *dataBuffer = G_io_apdu_buffer + OFFSET_CDATA;

    cx_ecfp_private_key_t privateKey;

    if (G_io_apdu_buffer[OFFSET_P1] != 0)
        THROW(0x6B00);

    if(G_io_apdu_buffer[OFFSET_P2] > 2)
        THROW(0x6B00);

    switch(G_io_apdu_buffer[OFFSET_P2]) {
        case 0:
            curve = CX_CURVE_Ed25519;
            break;
        case 1:
            curve = CX_CURVE_SECP256K1;
            break;
        case 2:
            curve = CX_CURVE_SECP256R1;
            break;
        default:
            THROW(0x6B00);
    }

    path_length = read_bip32_path(G_io_apdu_buffer[OFFSET_LC], bip32_path, dataBuffer);
    os_perso_derive_node_bip32(curve, bip32_path, path_length, privateKeyData, NULL);
    cx_ecfp_init_private_key(curve, privateKeyData, 32, &privateKey);
    cx_ecfp_generate_pair(curve, &public_key, &privateKey, 1);

    os_memset(&privateKey, 0, sizeof(privateKey));
    os_memset(privateKeyData, 0, sizeof(privateKeyData));

    if (curve == CX_CURVE_Ed25519) {
        cx_edward_compress_point(curve, public_key.W, public_key.W_len);
        public_key.W_len = 33;
    }

    if (instruction == INS_GET_PUBLIC_KEY) {
        return provide_pubkey();
    } else {
        // instruction == INS_PROMPT_PUBLIC_KEY || instruction == INS_AUTHORIZE_BAKING
        callback_t cb;
        const bagl_element_t *prompt;
        size_t prompt_size;
#ifdef BAKING_APP
        if (instruction == INS_AUTHORIZE_BAKING) {
            cb = baking_ok;
            prompt = ui_bake_prompt;
            prompt_size = sizeof(ui_bake_prompt)/sizeof(*ui_bake_prompt);
        } else {
#endif
            // INS_PROMPT_PUBLIC_KEY
            cb = pubkey_ok;
            prompt = ui_pubkey_prompt;
            prompt_size = sizeof(ui_pubkey_prompt)/sizeof(*ui_pubkey_prompt);
#ifdef BAKING_APP
        }
#endif
        prompt_address(prompt, prompt_size, public_key.W, public_key.W_len, cb, delay_reject);
        THROW(ASYNC_EXCEPTION);
    }
}

void prompt_address(const bagl_element_t *prompt, size_t prompt_size, void *raw_bytes,
                    uint32_t size, callback_t ok_cb, callback_t cxl_cb) {
    if (!convert_address(address_display_data, sizeof(address_display_data),
                         raw_bytes, size)) {
        uint16_t sz = 0x6B | (size & 0xFF);
        THROW(sz);
    }

    ux_step = 0;
    ux_step_count = 2;
    ui_prompt(prompt, prompt_size, ok_cb, cxl_cb, prompt_address_prepro);
}

const struct bagl_element_e *prompt_address_prepro(const struct bagl_element_e *element) {
    if (element->component.userid > 0) {
        unsigned int display = ux_step == element->component.userid - 1;
        if (display) {
            switch (element->component.userid) {
            case 1:
                UX_CALLBACK_SET_INTERVAL(500);
                break;
            case 2:
                UX_CALLBACK_SET_INTERVAL(MAX(
                    3000, 1000 + bagl_label_roundtrip_duration_ms(element, 7)));
                break;
            }
        }
        return (void*)display;
    }
    return (void*)1;
}
