#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "os.h"
#include "cx.h"
#include "ui.h"

int pubkey_to_pkh_string(char *buff, uint32_t buff_size, cx_curve_t curve,
                         const cx_ecfp_public_key_t *public_key);

// These functions do not output terminating null bytes, but instead
// return the ending offset.
size_t number_to_string(char *dest, uint64_t number);
size_t microtez_to_string(char *dest, uint64_t number);
