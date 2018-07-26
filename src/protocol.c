#include "protocol.h"

#include "apdu.h"
#include "ui.h"
#include "to_string.h"

#include <stdint.h>
#include <string.h>

#include "os.h"

struct __attribute__((__packed__)) block {
    char magic_byte;
    uint32_t chain_id;
    level_t level;
    uint8_t proto;
    // ... beyond this we don't care
};

bool is_block_valid(const void *data, size_t length) {
    if (length < sizeof(struct block)) return false;
    if (get_magic_byte(data, length) != MAGIC_BYTE_BLOCK) return false;
    // TODO: Check chain id
    return true;
}

level_t get_block_level(const void *data, __attribute__((unused)) size_t length) {
    check_null(data);
    const struct block *blk = data;
    return READ_UNALIGNED_BIG_ENDIAN(level_t, &blk->level);
}

struct operation_group_header {
    uint8_t magic_byte;
    uint8_t hash[32];
} __attribute__((packed));

struct contract {
    uint8_t outright;
    uint8_t curve_code;
    uint8_t pkh[HASH_SIZE];
} __attribute__((packed));

enum operation_tag {
    OPERATION_TAG_REVEAL = 7,
    OPERATION_TAG_TRANSACTION = 8,
    OPERATION_TAG_DELEGATION = 10,
};

struct operation_header {
    uint8_t tag;
    struct contract contract;
} __attribute__((packed));

struct delegation_contents {
    uint8_t delegate_present;
    uint8_t curve_code;
    uint8_t hash[HASH_SIZE];
} __attribute__((packed));

// These macros assume:
// * Beginning of data: const void *data
// * Total length of data: size_t length
// * Current index of data: size_t ix
// Any function that uses these macros should have these as local variables
#define NEXT_TYPE(type) ({ \
    if (ix + sizeof(type) > length) return sizeof(type); \
    const type *val = data + ix; \
    ix += sizeof(type); \
    val; \
})

#define NEXT_BYTE() (*NEXT_TYPE(uint8_t))

#define PARSE_Z() ({ \
    uint64_t acc = 0; \
    uint64_t shift = 0; \
    while (true) { \
        if (ix >= length) return 23; \
        uint8_t next_byte = NEXT_BYTE(); \
        acc |= (next_byte & 0x7F) << shift; \
        shift += 7; \
        if (!(next_byte & 0x80)) { \
            break; \
        } \
    } \
    acc; \
})

static void compute_pkh(cx_curve_t curve, size_t path_length, uint32_t *bip32_path,
                        struct parsed_operation_data *out) {
    cx_ecfp_public_key_t public_key_init;
    cx_ecfp_private_key_t private_key;
    generate_key_pair(curve, path_length, bip32_path, &public_key_init, &private_key);
    os_memset(&private_key, 0, sizeof(private_key));

    public_key_hash(out->hash, curve, &public_key_init, &out->public_key);

    switch (curve) {
        case CX_CURVE_Ed25519:
            out->curve_code = 0;
            break;
        case CX_CURVE_SECP256K1:
            out->curve_code = 1;
            break;
        case CX_CURVE_SECP256R1:
            out->curve_code = 2;
            break;
        default:
            THROW(EXC_MEMORY_ERROR);
    }
}

uint32_t parse_operations(const void *data, size_t length, cx_curve_t curve,
                          size_t path_length, uint32_t *bip32_path, struct parsed_operation_data *out) {
    check_null(data);
    check_null(bip32_path);
    check_null(out);
    memset(out, 0, sizeof(*out));

    compute_pkh(curve, path_length, bip32_path, out);

    size_t ix = 0;

    // Verify magic byte, ignore block hash
    const struct operation_group_header *ogh = NEXT_TYPE(struct operation_group_header);
    if (ogh->magic_byte != MAGIC_BYTE_UNSAFE_OP) return 15;

    while (ix < length) {
        const struct operation_header *hdr = NEXT_TYPE(struct operation_header);
        if (hdr->contract.curve_code != out->curve_code) return 13;
        if (hdr->contract.outright != 0) return 12;
        if (memcmp(hdr->contract.pkh, out->hash, HASH_SIZE) != 0) return 14;

        out->total_fee = PARSE_Z(); // fee
        PARSE_Z(); // counter
        PARSE_Z(); // gas limit
        PARSE_Z(); // storage limit
        switch (hdr->tag) {
            case OPERATION_TAG_REVEAL:
                // Public key up next!
                if (NEXT_BYTE() != out->curve_code) return 64;

                size_t klen = out->public_key.W_len;
                if (ix + klen > length) return klen;
                if (memcmp(out->public_key.W, data + ix, klen) != 0) return 4;
                ix += klen;
                break;
            case OPERATION_TAG_DELEGATION:
                {
                    // Delegation up next!
                    const struct delegation_contents *dlg = NEXT_TYPE(struct delegation_contents);
                    if (dlg->curve_code != out->curve_code) return 5;
                    if (dlg->delegate_present != 0xFF) return 6;
                    if (memcmp(dlg->hash, out->hash, HASH_SIZE) != 0) return 7;
                    out->contains_self_delegation = true;
                }
                break;
            case OPERATION_TAG_TRANSACTION:
                out->transaction_count++;
                out->amount += PARSE_Z();
                (void) NEXT_TYPE(struct contract);
                uint8_t params = NEXT_BYTE();
                if (params) return 101;
                break;
            default:
                return 8;
        }
    }

    return 0; // Success
}

#define TRANSACTION_BEGIN "Pay "
#define MAX_NUMBER_CHARS 21
char transaction_string[sizeof(TRANSACTION_BEGIN) + MAX_NUMBER_CHARS];

const bagl_element_t ui_sign_screen[] = {
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

    {{BAGL_LABELINE, 0x01, 0, 12, 128, 32, 0, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_EXTRABOLD_11px | BAGL_FONT_ALIGNMENT_CENTER, 0},
     "Tezos",
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
    {{BAGL_LABELINE, 0x01, 0, 26, 128, 32, 0, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_REGULAR_11px | BAGL_FONT_ALIGNMENT_CENTER, 0},
     transaction_string,
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
};

static void set_prompt_to_amount(uint64_t amount) {
    strcpy(transaction_string, TRANSACTION_BEGIN);
    size_t off = sizeof(TRANSACTION_BEGIN) - 1; // simulate strlen
    off += microtez_to_string(transaction_string + off, amount);
    transaction_string[off] = '\0';
}

// Return false if the transaction isn't easily parseable, otherwise prompt with given callbacks
// and do not return, but rather throw ASYNC.
bool prompt_transaction(const void *data, size_t length, cx_curve_t curve,
                        size_t path_length, uint32_t *bip32_path,
                        callback_t ok, callback_t cxl) {
    struct parsed_operation_data ops;
    uint32_t res = parse_operations(data, length, curve, path_length, bip32_path, &ops);

    if (res != 0) {
#ifdef DEBUG
        THROW(0x9000 + res);
#else
        return false;
#endif
    }

    if (ops.transaction_count != 1) return false;
    if (ops.contains_self_delegation) return false;
    if (ops.total_fee > 50000) return false;
    set_prompt_to_amount(ops.amount);
    ASYNC_PROMPT(ui_sign_screen, ok, cxl);
}
