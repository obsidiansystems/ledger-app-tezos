/*******************************************************************************
*   Ledger Blue
*   (c) 2016 Ledger
*
*  Licensed under the Apache License, Version 2.0 (the "License");
*  you may not use this file except in compliance with the License.
*  You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
*  Unless required by applicable law or agreed to in writing, software
*  distributed under the License is distributed on an "AS IS" BASIS,
*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*  See the License for the specific language governing permissions and
*  limitations under the License.
********************************************************************************/

#include "paths.h"

#ifdef TESTING
#include <stdlib.h>
#include <stdio.h>
#else
#include "os.h"
#endif

#include <stdbool.h>
#include <string.h>

#define MAX_INT_DIGITS 10

// This function does not output terminating null bytes.
uint32_t number_to_string(char *dest, uint32_t number) {
    char tmp[MAX_INT_DIGITS];
    char *const tmp_end = tmp + MAX_INT_DIGITS;
    char *ptr;
    for (ptr = tmp_end - 1; ptr >= tmp; ptr--) {
        *ptr = '0' + number % 10;
        number /= 10;
        if (number == 0) {
            break;
        }
    }
    int length = tmp_end - ptr;
    memcpy(dest, ptr, length);
    return length;
}

#define HARDENING_BIT (1u << 31)

// This function does not output terminating null bytes
static uint32_t path_item_to_string(char *dest, uint32_t input) {
    int length = number_to_string(dest, input & ~HARDENING_BIT);
    if (input & HARDENING_BIT) {
        dest[length] = '\'';
        length++;
    }
    return length;
}

// This function does output terminating null bytes
uint32_t path_to_string(char *buf, uint32_t path_length, uint32_t *bip32_path) {
    uint32_t offset = 0;
    for (uint32_t i = 0; i < path_length; i++) {
        offset += path_item_to_string(buf + offset, bip32_path[i]);
        if (i != path_length - 1) {
            buf[offset++] = '/';
        }
    }
    buf[offset++] = '\0';
    return offset;
}

uint32_t read_bip32_path(uint32_t bytes, uint32_t *bip32_path, const uint8_t *buf) {
    uint32_t path_length = *buf;
    if (bytes < path_length * sizeof(uint32_t) + 1) THROW(0x6B00);

    buf++;

    if (path_length == 0 || path_length > MAX_BIP32_PATH) {
#ifdef TESTING
        printf("Invalid path\n");
        abort();
#else
        screen_printf("Invalid path\n");
        THROW(0x6a80);
#endif
    }

    for (uint32_t i = 0; i < path_length; i++) {
        bip32_path[i] = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | (buf[3]);
        buf += 4;
    }

    return path_length;
}

#ifdef TESTING
int main() {
    uint32_t bip32_path[MAX_BIP32_PATH];
    uint32_t size;
    bip32_path[0] = 33;
    bip32_path[1] = 45;
    bip32_path[2] = 0x80000202;
    size = 3;
    char buff[80];
    int sz = path_to_string(buff, size, bip32_path);
    // Intended output:
    // [11]: 33/45/514'
    printf("[%d]: %s\n", sz, buff);
}
#endif
