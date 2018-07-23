#include "protocol.h"

#include "apdu.h"

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

level_t get_block_level(const void *data, size_t length) {
    check_null(data);
    const struct block *blk = data;
    return READ_UNALIGNED_BIG_ENDIAN(level_t, &blk->level);
}

static inline const void *next_bytes(const void *data, size_t length, size_t *ix, size_t data_len) {
    check_null(ix);
    const uint8_t *bytes = data;
    if (*ix + data_len > length) return NULL;
    const void *res = bytes + *ix;
    *ix += data_len;
    return res;
}

static inline uint64_t parse_z(const uint8_t *bytes, size_t length, size_t *pos) {
    check_null(bytes);
    check_null(pos);
    uint64_t acc = 0;
    uint32_t shift = 0;
    while (true) {
        uint8_t next_byte = *(uint8_t*)next_bytes(bytes, length, pos, 1);
        acc |= (next_byte & 0x7F) << shift;
        shift += 7;
        if (!(next_byte & 0x80)) { // last byte has no high bit set
            return acc;
        }
    }
}

// These macros assume:
// * Beginning of data: const void *data
// * Total length of data: size_t length
// * Current index of data: size_t ix
// Any function that uses these macros should have these as local variables
#define NEXT_TYPE(type) ({ \
    const type *val = next_bytes(data, length, &ix, sizeof(type)); \
    if (val == NULL) { \
        return sizeof(type); \
    } \
    val; \
})
#define NEXT_BYTE() (*NEXT_TYPE(uint8_t))
#define PARSE_Z() parse_z(data, length, &ix)

typedef uint32_t (*ops_parser)(const void *data, size_t length, size_t *ix_p, uint8_t tag, uint64_t fee);

// Shared state between ops parser and main parser
static cx_ecfp_public_key_t public_key;
static cx_ecfp_private_key_t private_key;
static uint8_t hash[HASH_SIZE];
static uint8_t curve_code;

// Zero means correct parse, non-zero means problem
// Specific return code can be used for debugging purposes
uint32_t parse_operations(const void *data, size_t length, cx_curve_t curve,
                          size_t path_length, uint32_t *bip32_path, ops_parser parser) {
    check_null(data);
    check_null(bip32_path);

    cx_ecfp_public_key_t public_key_init;

    generate_key_pair(curve, path_length, bip32_path, &public_key_init, &private_key);
    os_memset(&private_key, 0, sizeof(private_key));

    public_key_hash(hash, curve, &public_key_init, &public_key);

    switch (curve) {
        case CX_CURVE_Ed25519:
            curve_code = 0;
            break;
        case CX_CURVE_SECP256K1:
            curve_code = 1;
            break;
        case CX_CURVE_SECP256R1:
            curve_code = 2;
            break;
        default:
            return 9; // Should not be reached
    }

    size_t ix = 0;

    // Verify magic byte, ignore block hash
    const struct operation_group_header *ogh = NEXT_TYPE(struct operation_group_header);
    if (ogh->magic_byte != MAGIC_BYTE_UNSAFE_OP) return 15;

    while (ix < length) {
        const struct operation_header *hdr = NEXT_TYPE(struct operation_header);
        if (hdr->contract.curve_code != curve_code) return 13;
        if (hdr->contract.outright != 0) return 12;
        if (memcmp(hdr->contract.pkh, hash, HASH_SIZE) != 0) return 14;

        uint64_t fee = PARSE_Z(); // fee
        PARSE_Z(); // counter
        PARSE_Z(); // gas limit
        PARSE_Z(); // storage limit

        uint32_t res = parser(data, length, &ix, hdr->tag, fee);
        if (res != 0) return res;
    }
}

static uint32_t self_delg_parser(const void *data, size_t length, size_t *ix_p, uint8_t tag,
                                 uint64_t fee) {
    size_t ix = *ix_p;

    switch (tag) {
        case OPERATION_TAG_REVEAL:
            if (fee != 0) return 3; // Who sets a fee for reveal?

            // Public key up next!
            if (NEXT_BYTE() != curve_code) return 64;
            const uint8_t *pubkey = next_bytes(data, length, &ix, public_key.W_len);
            if (memcmp(public_key.W, pubkey, public_key.W_len) != 0) return 4;
            break;
        case OPERATION_TAG_DELEGATION:
            if (fee > 50000) return 100; // You want a higher fee, use wallet app!

            // Delegation up next!
            const struct delegation_contents *dlg = NEXT_TYPE(struct delegation_contents);
            if (dlg->curve_code != curve_code) return 5;
            if (dlg->delegate_present != 0xFF) return 6;
            if (memcmp(dlg->hash, hash, HASH_SIZE) != 0) return 7;
            break;
        default:
            return 8;
    }

    *ix_p = ix;
    return 0;
}


void guard_valid_self_delegation(const void *data, size_t length, cx_curve_t curve,
                                 size_t path_length, uint32_t *bip32_path) {
    if (parse_operations(data, length, curve, path_length, bip32_path, self_delg_parser) == 0) {
        return; // Not throwing indicates success
    } else {
        THROW(EXC_PARSE_ERROR);
    }
}

char transaction_string[80];

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
#define TRANSACTION_BEGIN "Transaction for "
    strcpy(transaction_string, TRANSACTION_BEGIN);
    size_t off = sizeof(TRANSACTION_BEGIN);
    off += number_to_string(transaction_string + off, amount);
    transaction_string[off] = '\0';
}

static uint32_t transact_parser(const void *data, size_t length, size_t *ix_p, uint8_t tag,
                                uint64_t fee) {
    size_t ix = *ix_p;

    switch (tag) {
        case OPERATION_TAG_TRANSACTION:
            if (fee > 50000) return 100;
            uint64_t amount = PARSE_Z();
            set_prompt_to_amount(amount);
            break;
        default:
            return 8;
    }

    *ix_p = ix;
    return 0;
}

// Return false if the transaction isn't easily parseable, otherwise prompt with given callbacks
// and do not return, but rather throw ASYNC.
bool prompt_transaction(const void *data, size_t length, cx_curve_t curve,
                        size_t path_length, uint32_t *bip32_path,
                        callback_t ok, callback_t cxl) {
    if (parse_operations(data, length, curve, path_length, bip32_path, transact_parser) == 0) {
        ASYNC_PROMPT(ui_sign_screen, ok, cxl);
    } else {
        return false;
    }
}
