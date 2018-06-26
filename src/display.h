#pragma once

#include <stdint.h>

#include "os.h"
#include "cx.h"

int convert_address(char *buff, uint32_t buff_size, cx_curve_t curve,
                    const cx_ecfp_public_key_t *public_key);
