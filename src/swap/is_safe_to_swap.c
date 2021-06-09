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
        PRINTF("Expected a babylon transaction, got %d\n", op->operation.tag);
        return false;
    } else if (op->signing.signature_type != SIGNATURE_TYPE_ED25519) {
        PRINTF("Expected type ED25519 signature, got %d\n", op->signing.signature_type);
        return false;
    } else if (op->total_fee != swap_values.fees) {
        PRINTF("Fees differ: expected %d, got %d\n", op->total_fee, swap_values.fees);
        return false;
    } else if (op->operation.amount != swap_values.amount) {
        PRINTF("Amounts differ: expected %d, got %d\n", op->operation.amount, swap_values.amount);
        return false;
    } else if (strncmp((const char *) &tmp_dest, swap_values.destination, sizeof(tmp_dest))) {
        PRINTF("Addresses differ: expected %.*H, got %.*H\n",
               sizeof(tmp_dest),
               tmp_dest,
               sizeof(tmp_dest),
               swap_values.destination);
        return false;
    }
    return true;
}
