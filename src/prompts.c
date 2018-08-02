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

#define MAX_NUMBER_CHARS (MAX_INT_DIGITS + 2) // include decimal point and terminating null

// Return false if the transaction isn't easily parseable, otherwise prompt with given callbacks
// and do not return, but rather throw ASYNC.
bool prompt_transaction(const void *data, size_t length, cx_curve_t curve,
                        size_t path_length, uint32_t *bip32_path,
                        callback_t ok, callback_t cxl) {
    struct parsed_operation_group *ops;

#ifndef TEZOS_DEBUG
    BEGIN_TRY { // TODO: Eventually, "unsafe" operations will be another APDU,
                //       and we will parse enough operations that it will rarely need to be used,
                //       hopefully ultimately never.
        TRY {
#endif
            allowed_operation_set allowed;
            clear_operation_set(&allowed);
            allow_operation(&allowed, OPERATION_TAG_DELEGATION);
            allow_operation(&allowed, OPERATION_TAG_REVEAL);
            allow_operation(&allowed, OPERATION_TAG_ORIGINATION);
            allow_operation(&allowed, OPERATION_TAG_TRANSACTION);
            // TODO: Add other operations

            ops = parse_operations(data, length, curve, path_length, bip32_path, allowed);
#ifndef TEZOS_DEBUG
        }
        CATCH_OTHER(e) {
            return false;
        }
        FINALLY { }
    }
    END_TRY;
#endif

    // If the source is an implicit contract,...
    if (ops->operation.source.originated == 0) {
        // ... it had better match our key, otherwise why are we signing it?
        if (memcmp(&ops->operation.source, &ops->signing, sizeof(ops->signing))) return false;
    }
    // OK, it passes muster.

    // Now to display it to make sure it's what the user intended.
    static char origin_string[40];
    static char destination_string[40];
    static char fee_string[MAX_NUMBER_CHARS];

    microtez_to_string(fee_string, ops->total_fee);
    if (!parsed_contract_to_string(origin_string, sizeof(origin_string),
                                   &ops->operation.source)) return false;
    if (!parsed_contract_to_string(destination_string, sizeof(destination_string),
                                   &ops->operation.destination)) return false;

    switch (ops->operation.tag) {
        default:
            THROW(EXC_PARSE_ERROR);

        case OPERATION_TAG_ORIGINATION:
            {
                static char delegate_string[40];
                static const char *const origination_prompts[] = {
                    "Approve",
                    "Source",
                    "Manager",
                    "Fee",
                    "Delegatable?",
                    NULL,
                };

                static const char *const origination_values[] = {
                    "Origination?",
                    origin_string,
                    destination_string,
                    fee_string,
                    delegate_string,
                    NULL,
                };

                if (!(ops->operation.flags & ORIGINATION_FLAG_SPENDABLE)) return false;
                bool delegatable = ops->operation.flags & ORIGINATION_FLAG_DELEGATABLE;

                strcpy(delegate_string, delegatable ? "Yes" : "No");

                ui_prompt_multiple(origination_prompts, origination_values, ok, cxl);
            }
        case OPERATION_TAG_DELEGATION:
            {
                static const char *const delegation_prompts[] = {
                    "Approve",
                    "Source",
                    "Delegate",
                    "Fee",
                    NULL,
                };

                static const char *const delegation_values[] = {
                    "Delegation?",
                    origin_string,
                    destination_string,
                    fee_string,
                    NULL,
                };

                ui_prompt_multiple(delegation_prompts, delegation_values, ok, cxl);
            }

        case OPERATION_TAG_TRANSACTION:
            {
                static char amount_string[MAX_NUMBER_CHARS];

                static const char *const transaction_prompts[] = {
                    "Approve",
                    "Source",
                    "Destination",
                    "Amount",
                    "Fee",
                    NULL,
                };

                static const char *const transaction_values[] = {
                    "Transaction?",
                    origin_string,
                    destination_string,
                    amount_string,
                    fee_string,
                    NULL,
                };

                microtez_to_string(amount_string, ops->operation.amount);
                ui_prompt_multiple(transaction_prompts, transaction_values, ok, cxl);
            }
    }
}
