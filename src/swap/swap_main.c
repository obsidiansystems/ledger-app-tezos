#include "swap_lib_calls.h"
#include "os.h"

static void library_main_helper(struct libargs_s *args) {
    check_api_level(CX_COMPAT_APILEVEL);
    PRINTF("Inside library \n");
    switch (args->command) {
        case CHECK_ADDRESS:
            // ensure result is zero if an exception is thrown
            args->check_address->result = 0;
            args->check_address->result = handle_check_address(args->check_address);
            break;
        case SIGN_TRANSACTION:
            if (copy_transaction_parameters(args->create_transaction)) {
                // never returns
                handle_swap_sign_transaction();
            }
            break;
        case GET_PRINTABLE_AMOUNT:
            handle_get_printable_amount(args->get_printable_amount);
            break;
        default:
            break;
    }
}

void library_main(struct libargs_s *args) {
    bool end = false;
    /* This loop ensures that library_main_helper and os_lib_end are called
     * within a try context, even if an exception is thrown */
    while (1) {
        BEGIN_TRY {
            TRY {
                if (!end) {
                    library_main_helper(args);
                }
                os_lib_end();
            }
            FINALLY {
                end = true;
            }
        }
        END_TRY;
    }
}