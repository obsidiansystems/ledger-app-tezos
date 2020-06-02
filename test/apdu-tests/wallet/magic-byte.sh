#!/usr/bin/env bash
set -euo pipefail

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$DIR"

# NOTE: THIS TEST MUST BE RUN IN THE WALLET APP
LAST_MSG="81"
# BIP 32 path APDU
DERIV_MSG="800f000311048000002c800006c18000000080000000"
INS_SIGN="04"
INS_SIGN_WITH_HASH="0f"

# tezos-client hash data '"hello world"' of type string
MSG="010000000b68656c6c6f20776f726c64"

# tezos-client sign bytes <data labeled "Raw packed data" from prior cmd> for <my-ledger>
## The purpose of these here tests is to establish that the tezos-wallet app
## behaves correctly upon receiving messages with varying magic-bytes
{
  echo; echo "SignWithHash and Sign requests containing magic byte 0x05 should always be 'Sign Hash'"
  echo; echo "Confirm that the next two tests say: "
  echo; echo "Expect: 'Unrecognized Michelson: Sign Hash:...'"

  {
    DATA="05$MSG"
    DATA_LENGTH="11"

    # Expect: Unrecognized Michelson: Sign Hash:
    echo $DERIV_MSG
    echo 80${INS_SIGN_WITH_HASH}${LAST_MSG}00${DATA_LENGTH}${DATA}

    # Expect: Unrecognized Michelson: Sign Hash:
    echo $DERIV_MSG
    echo 80${INS_SIGN}${LAST_MSG}00${DATA_LENGTH}${DATA}
  } | ../apdu.sh
}

{
  echo; echo "SignWithHash and Sign requests containing magic byte 0x03 should parse but fallback to 'Sign Hash'"
  echo; echo "Confirm that the next two tests say: "
  echo; echo "Expect: 'Unrecognized Operation: Sign Hash:...'"

    DATA="03$MSG"
    DATA_LENGTH="11"

  {
    # Expect: Unrecognized Operation: Sign Hash:
    echo $DERIV_MSG
    echo 80${INS_SIGN_WITH_HASH}${LAST_MSG}00${DATA_LENGTH}${DATA}
  } | ../apdu.sh

echo; echo "Expect: 'Unrecognized Operation: Sign Hash:...'"
  {
    # Expect: Unrecognized Operation: Sign Hash:
    echo $DERIV_MSG
    echo 80${INS_SIGN}${LAST_MSG}00${DATA_LENGTH}${DATA}
  } | ../apdu.sh
}
{
  echo; echo "SignWithHash and Sign requests containing magic byte 0x04 fail"
  echo; echo "Expecting failure"

  {
    DATA="04$MSG"
    DATA_LENGTH="11"

    echo $DERIV_MSG
    echo 80${INS_SIGN_WITH_HASH}${LAST_MSG}00${DATA_LENGTH}${DATA}

  } | ../apdu.sh
}
