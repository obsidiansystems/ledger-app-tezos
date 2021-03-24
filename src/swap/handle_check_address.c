#include "swap_lib_calls.h"
#include "globals.h"
#include "to_string.h"

// Returns 0 if there is an error, else returns 1.
int handle_check_address(const check_address_parameters_t* params) {
    PRINTF("Params on the address %d\n", (unsigned int) params);
    PRINTF("Address to check %s\n", params->address_to_check);
    PRINTF("Inside handle_check_address\n");

    if (params->address_to_check == 0) {
        PRINTF("Address to check == 0\n");
        return 0;
    }

    bip32_path_t bip32_path = {0};
    size_t const s = READ_UNALIGNED_BIG_ENDIAN(uint8_t, &params->address_parameters_length);

    read_bip32_path(&bip32_path, params->address_parameters, s);
    derivation_type_t derivation_type = 3;
    cx_ecfp_public_key_t public_key = {0};
    int error = generate_public_key(&public_key, derivation_type, &bip32_path);
    if (error) {
        PRINTF("Error generating public_key\n");
        return 0;
    }

    char address[57];
    pubkey_to_pkh_string(address, sizeof(address), derivation_type, &public_key);

    if (strcmp(address, params->address_to_check) != 0) {
        PRINTF("Addresses do not match\n");
        return 0;
    }

    PRINTF("Addresses match\n");
    return 1;
}
