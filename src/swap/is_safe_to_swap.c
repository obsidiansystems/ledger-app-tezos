#include "swap_lib_calls.h"
#include "os_io_seproxyhal.h"
#include "apdu.h"
#include "globals.h"
#include "to_string.h"

bool is_safe_to_swap() {
    struct parsed_operation_group *op = &global.apdu.u.sign.maybe_ops.v;
    char tmp_dest[57] = {0};
    parsed_contract_to_string(tmp_dest, sizeof(tmp_dest), &op->operation.destination);

    if (op->signing.originated != 0) {
        PRINTF("Should not be originated\n");
        return false;
    } else if (op->operation.tag != OPERATION_TAG_BABYLON_TRANSACTION) {
        PRINTF("Should be of type babylon transaction\n");
        return false;
    } else if (op->signing.signature_type != SIGNATURE_TYPE_ED25519) {
        PRINTF("Signature type is not ED25519\n");
        return false;
    } else if (op->total_storage_limit >= 257) {
        PRINTF("Storage Limit incorrect\n");
        return false;
    } else if (op->total_fee != swap_values.fees) {
        PRINTF("Fees differ\n");
        return false;
    } else if (op->operation.amount != swap_values.amount) {
        PRINTF("Amounts differ\n");
        return false;
    } else if (strncmp((const char *) &tmp_dest, swap_values.destination, sizeof(tmp_dest))) {
        PRINTF("Addresses differ\n");
        return false;
    }
    return true;
}
