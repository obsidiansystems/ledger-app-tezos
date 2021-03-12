#include <string.h>
#include <stdint.h>
#include "to_string.h"

#include "swap_lib_calls.h"

/* return 0 on error, 1 otherwise */
int handle_get_printable_amount(get_printable_amount_parameters_t* params) {
    uint64_t amount;
    params->printable_amount[0] = '\0';

    if (swap_str_to_u64(params->amount, params->amount_length, &amount) == 0) {
        PRINTF("Amount is too big");
        return 0;
    }

    if (microtez_to_string_indirect_no_throw(&params->printable_amount, sizeof(params->printable_amount), &amount) == 0) {
        PRINTF("Error converting number\n");
        return 0;
    }
    return 1;
}