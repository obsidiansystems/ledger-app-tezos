# APDU

An APDU is sent by a client to the ledger hardware. This message tells
the ledger some operation to run. Each APDU message will be
accompanied by an Accept / Deny prompt on the Ledger. Once the user
hits “Accept” the Ledger will issue a response to the Tezos client.
The basic format of the APDU request follows.

| Field | Length | Description                                                             |
|-------|--------|-------------------------------------------------------------------------|
| CLA   | 1 byte | Instruction class (always 0x80)                                         |
| INS   | 1 byte | Instruction code (0x00-0x0f)                                            |
| P1    | 1 byte | User-defined 1-byte parameter                                           |
| P2    | 1 byte | Derivation type (0=ED25519, 1=SECP256K1, 2=SECP256R1, 3=BIPS32_ED25519) |
| LC    | 1 byte | Length of CDATA                                                         |
| CDATA | <LC>   | Payload containing instruction arguments                                |

Each APDU has a header of 5 bytes followed by some data. The format of
the data will depend on which instruction is being used.

## Example

Here is an example of an APDU message from the ledger-app-tezos tests:

> 0x8001000011048000002c800006c18000000080000000

This parses as:

| Field | Value                                |
|-------|--------------------------------------|
| CLA   | 0x80                                 |
| INS   | 0x01                                 |
| P1    | 0x00                                 |
| P2    | 0x00                                 |
| LC    | 0x11 (17)                            |
| CDATA | 0x048000002c800006c18000000080000000 |

0x01 is the first instruction and is used for "authorize baking". This
APDU asks the ledger to use its internal private keys to sign an
authorization to bake. Note that this instruction is only recognized
by the baking app, and not the wallet app.

A list of more instructions follows.

## APDU instructions in use by Tezos Ledger apps

| Instruction                     | Code | App | Short description                                |
|---------------------------------|------|-----|--------------------------------------------------|
| `INS_VERSION`                   | 0x00 | WB  | Get version information for the ledger           |
| `INS_AUTHORIZE_BAKING`          | 0x01 | B   | Authorize baking                                 |
| `INS_GET_PUBLIC_KEY`            | 0x02 | WB  | Get the ledger’s internal public key             |
| `INS_PROMPT_PUBLIC_KEY`         | 0x03 | WB  | Prompt for the ledger’s internal public key      |
| `INS_SIGN`                      | 0x04 | WB  | Sign a message with the ledger’s key             |
| `INS_SIGN_UNSAFE`               | 0x05 | W   | Sign a message with the ledger’s key (no hash)   |
| `INS_RESET`                     | 0x06 | B   | Reset high water mark block level                |
| `INS_QUERY_AUTH_KEY`            | 0x07 | B   | Get auth key                                     |
| `INS_QUERY_MAIN_HWM`            | 0x08 | B   | Get current high water mark                      |
| `INS_GIT`                       | 0x09 | WB  | Get the commit hash                              |
| `INS_SETUP`                     | 0x0a | B   | Setup a baking address                           |
| `INS_QUERY_ALL_HWM`             | 0x0b | B   | Get all high water mark information              |
| `INS_DEAUTHORIZE`               | 0x0c | B   | Unauthorize baking                               |
| `INS_QUERY_AUTH_KEY_WITH_CURVE` | 0x0d | B   | Get auth key and curve                           |
| `INS_HMAC`                      | 0x0e | B   | Get the HMAC of a message                        |
| `INS_SIGN_WITH_HASH`            | 0x0f | WB  | Sign a message with the ledger’s key (with hash) |
