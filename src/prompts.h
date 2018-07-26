#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "os.h"
#include "cx.h"
#include "ui.h"

void prompt_address(bool bake, cx_curve_t curve,
                    const cx_ecfp_public_key_t *key,
                    callback_t ok_cb, callback_t cxl_cb) __attribute__((noreturn));

// Return false if the transaction isn't easily parseable, otherwise prompt with given callbacks
// and do not return, but rather throw ASYNC.
bool prompt_transaction(const void *data, size_t length, cx_curve_t curve,
                        size_t path_length, uint32_t *bip32_path,
                        callback_t ok, callback_t cxl);
