#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "os.h"
#include "cx.h"
#include "ui.h"


int convert_address(char *buff, uint32_t buff_size, cx_curve_t curve,
                    const cx_ecfp_public_key_t *public_key);
void prompt_address(bool bake, cx_curve_t curve,
                    const cx_ecfp_public_key_t *key, callback_t ok_cb, callback_t cxl_cb);
