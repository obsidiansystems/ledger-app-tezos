#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "os.h"
#include "cx.h"
#include "ui.h"

void prompt_address(bool bake, cx_curve_t curve,
                    const cx_ecfp_public_key_t *key,
                    callback_t ok_cb, callback_t cxl_cb) __attribute__((noreturn));
