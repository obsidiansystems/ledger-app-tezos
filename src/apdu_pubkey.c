#include "apdu_pubkey.h"

#include "apdu.h"
#include "baking_auth.h"
#include "display.h"

#include "cx.h"
#include "ui.h"

static cx_ecfp_public_key_t public_key;

// The following need to be persisted for baking app
static uint8_t path_length;
static uint32_t bip32_path[MAX_BIP32_PATH];

static const struct bagl_element_e *prompt_address_prepro(const struct bagl_element_e *element);
static void prompt_address(void *raw_bytes, uint32_t size, callback_t ok_cb, callback_t cxl_cb);

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
#ifdef BAKING_APP
    authorize_baking(NULL, 0, bip32_path, path_length);
#endif

    int tx = provide_pubkey();
    delay_send(tx);
}

unsigned int handle_apdu_get_public_key(uint8_t instruction) {
    uint8_t privateKeyData[32];
    uint8_t *dataBuffer = G_io_apdu_buffer + OFFSET_CDATA;

    cx_curve_t curve;
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

    path_length = read_bip32_path(bip32_path, dataBuffer);
    os_perso_derive_node_bip32(curve, bip32_path, path_length, privateKeyData, NULL);
    cx_ecfp_init_private_key(curve, privateKeyData, 32, &privateKey);
    cx_ecfp_generate_pair(curve, &public_key, &privateKey, 1);

    os_memset(&privateKey, 0, sizeof(privateKey));
    os_memset(privateKeyData, 0, sizeof(privateKeyData));

    if (curve == CX_CURVE_Ed25519) {
        cx_edward_compress_point(curve, public_key.W, public_key.W_len);
        public_key.W_len = 33;
    }

#ifdef BAKING_APP
    if (is_path_authorized(bip32_path, path_length)) {
        return provide_pubkey();
    }
#endif

    prompt_address(public_key.W, public_key.W_len, pubkey_ok, delay_reject);
    THROW(ASYNC_EXCEPTION);
}

#define ADDRESS_BYTES 65
char address_display_data[ADDRESS_BYTES * 2 + 1];

const bagl_element_t ui_display_address[] = {
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
#ifdef BAKING_APP
     "Authorize baking",
#else
     "Provide",
#endif
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
    {{BAGL_LABELINE, 0x01, 0, 26, 128, 12, 0, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_EXTRABOLD_11px | BAGL_FONT_ALIGNMENT_CENTER, 0},
#ifdef BAKING_APP
     "with public key?",
#else
     "public key?",
#endif
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

// TODO: Split into two functions
void prompt_address(void *raw_bytes, uint32_t size, callback_t ok_cb, callback_t cxl_cb) {
    if (!convert_address(address_display_data, sizeof(address_display_data),
                         raw_bytes, size)) {
        uint16_t sz = 0x6B | (size & 0xFF);
        THROW(sz);
    }

    ui_prompt(ui_display_address, sizeof(ui_display_address)/sizeof(*ui_display_address),
              ok_cb, cxl_cb, prompt_address_prepro);
    ux_step = 0;
    ux_step_count = 2;
}

const struct bagl_element_e *prompt_address_prepro(const struct bagl_element_e *element) {
    if (element->component.userid > 0) {
        unsigned int display = ux_step == element->component.userid - 1;
        if (display) {
            switch (element->component.userid) {
            case 1:
                UX_CALLBACK_SET_INTERVAL(2000);
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
