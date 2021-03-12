#include "os_io_seproxyhal.h"
#include "swap_lib_calls.h"
#include "ux.h"

bool copy_transaction_parameters(const create_transaction_parameters_t* params) {
    // first copy parameters to stack, and then to global data.
    // We need this "trick" as the input data position can overlap with btc-app globals
    swap_values_t stack_data;
    memset(&stack_data, 0, sizeof(stack_data));

    if (strlen(params->destination_address) >= sizeof(stack_data.destination) ||
        strlen(params->destination_address_extra_id) >= sizeof(stack_data.memo)) {
        return false;
    }
    strlcpy(stack_data.destination, params->destination_address, sizeof(stack_data.destination));
    strlcpy(stack_data.memo, params->destination_address_extra_id, sizeof(stack_data.memo));

    if (!swap_str_to_u64(params->amount, params->amount_length, &stack_data.amount)) {
        return false;
    }

    if (!swap_str_to_u64(params->fee_amount, params->fee_amount_length, &stack_data.fees)) {
        return false;
    }

    memcpy(&swap_values, &stack_data, sizeof(stack_data));

    return true;
}

void handle_swap_sign_transaction(void) {
    called_from_swap = true;
    // reset_ctx(); scott
    io_seproxyhal_init();
    UX_INIT();
    USB_power(0);
    USB_power(1);
    // ui_idle();
    PRINTF("USB power ON/OFF\n");
#ifdef TARGET_NANOX
    // grab the current plane mode setting
    G_io_app.plane_mode = os_setting_get(OS_SETTING_PLANEMODE, NULL, 0);
#endif  // TARGET_NANOX
#ifdef HAVE_BLE
    BLE_power(0, NULL);
    BLE_power(1, "Nano X");
#endif  // HAVE_BLE
    app_main();
}
