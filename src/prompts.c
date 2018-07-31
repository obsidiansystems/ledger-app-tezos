#include "prompts.h"

#include "apdu.h"
#include "base58.h"
#include "blake2.h"
#include "keys.h"
#include "protocol.h"
#include "to_string.h"
#include "ui_prompt.h"

#include "os.h"
#include "cx.h"
#include "ui.h"

#include <string.h>

static char address_display_data[64]; // Should be more than big enough

static const char *const pubkey_labels[] = {
    "Provide",
    "Public Key",
    NULL,
};

static const char *const pubkey_values[] = {
    "Public Key?",
    address_display_data,
    NULL,
};

#ifdef BAKING_APP
static const char *const baking_labels[] = {
    "Authorize baking",
    "Public Key",
    NULL,
};

static const char *const baking_values[] = {
    "With Public Key?",
    address_display_data,
    NULL,
};

// TODO: Unshare code with next function
void prompt_contract_for_baking(struct parsed_contract *contract, callback_t ok_cb, callback_t cxl_cb) {
    if (!parsed_contract_to_string(address_display_data, sizeof(address_display_data), contract)) {
        THROW(EXC_WRONG_VALUES);
    }

    ui_prompt_multiple(baking_labels, baking_values, ok_cb, cxl_cb);
}
#endif

void prompt_address(
#ifndef BAKING_APP
    __attribute__((unused))
#endif
    bool baking,
                    cx_curve_t curve, const cx_ecfp_public_key_t *key, callback_t ok_cb,
                    callback_t cxl_cb) {
    if (!pubkey_to_pkh_string(address_display_data, sizeof(address_display_data), curve, key)) {
        THROW(EXC_WRONG_VALUES);
    }

#ifdef BAKING_APP
    if (baking) {
        ui_prompt_multiple(baking_labels, baking_values, ok_cb, cxl_cb);
    } else {
#endif
        ui_prompt_multiple(pubkey_labels, pubkey_values, ok_cb, cxl_cb);
#ifdef BAKING_APP
    }
#endif
}

#define MAX_NUMBER_CHARS 21 // include decimal point
static char origin_string[40];
static char destination_string[40];
static char amount_string[MAX_NUMBER_CHARS + 1]; // include terminating null
static char fee_string[MAX_NUMBER_CHARS + 1];

static const char *const transaction_prompts[] = {
    "Source",
    "Destination",
    "Amount",
    "Fee",
    NULL,
};

static const char *const transaction_values[] = {
    origin_string,
    destination_string,
    amount_string,
    fee_string,
    NULL,
};

// Return false if the transaction isn't easily parseable, otherwise prompt with given callbacks
// and do not return, but rather throw ASYNC.
bool prompt_transaction(const void *data, size_t length, cx_curve_t curve,
                        size_t path_length, uint32_t *bip32_path,
                        callback_t ok, callback_t cxl) {
    struct parsed_operation_group ops;
    uint32_t res = parse_operations(data, length, curve, path_length, bip32_path, &ops);

    if (res != 0) {
#ifdef DEBUG
        THROW(0x9000 + res);
#else
        return false;
#endif
    }

    // Ensure we have one transaction (and possibly a reveal).
    const struct parsed_operation *transaction =
        find_sole_unsafe_operation(&ops, OPERATION_TAG_TRANSACTION);
    if (transaction == NULL) return false;

    // If the source is an implicit contract,...
    if (transaction->source.originated == 0) {
        // ... it had better match our key, otherwise why are we signing it?
        if (memcmp(&transaction->source, &ops.signing, sizeof(ops.signing))) return false;
    }
    // OK, it passes muster.

    // Now to display it to make sure it's what the user intended.
    microtez_to_string(amount_string, transaction->amount);
    microtez_to_string(fee_string, ops.total_fee);
    if (!parsed_contract_to_string(origin_string, sizeof(origin_string),
                                   &transaction->source)) return false;
    if (!parsed_contract_to_string(destination_string, sizeof(destination_string),
                                   &transaction->destination)) return false;

    ui_prompt_multiple(transaction_prompts, transaction_values, ok, cxl);
}
