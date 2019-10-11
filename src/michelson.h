// Subset of Michelson constants, used in parsing manager.tz
// operations.

#pragma once

#include "memory.h"

#define MICHELSON_ADDRESS          0x036e
#define MICHELSON_CONS             0x031b
#define MICHELSON_DROP             0x0320
#define MICHELSON_IMPLICIT_ACCOUNT 0x031e
#define MICHELSON_KEY_HASH         0x035d
#define MICHELSON_MUTEZ            0x036a
#define MICHELSON_NIL              0x053d
#define MICHELSON_NONE             0x053e
#define MICHELSON_OPERATION        0x036d
#define MICHELSON_PUSH             0x0743
#define MICHELSON_SET_DELEGATE     0x034e
#define MICHELSON_SOME             0x0346
#define MICHELSON_TRANSFER_TOKENS  0x034d
#define MICHELSON_UNIT             0x034f
#define MICHELSON_IF_NONE          0x072f
#define MICHELSON_FAILWITH         0x0327

#define MICHELSON_CONTRACT         0x0555
#define MICHELSON_CONTRACT_WITH_ENTRYPOINT 0x0655

#define MICHELSON_TYPE_BYTE_SEQUENCE 0x0a
#define MICHELSON_TYPE_STRING        0x01
#define MICHELSON_TYPE_SEQUENCE      0x02
#define MICHELSON_TYPE_UNIT          0x4f

#define MICHELSON_PARAMS_SOME   0xff
#define MICHELSON_PARAMS_NONE   0x00

#define MAX_MICHELSON_SEQUENCE_LENGTH 100
