#!/usr/bin/env bash
set -Eeuo pipefail

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$DIR"

# tezos-client transfer 1 from <my-ledger> to <other tz1 account> --burn-cap 0.257
# fee = 0.001283
# storage limit = 0
CONTROL_MSG="17777d8de5596705f1cb35b0247b9605a7c93a7ed5c0caa454d4f4ff39eb411d6c00cf49f66b9ea137e11818f2a78b4b6fc9895b4e50830ae58003c35000c0843d0000eac6c762212c4110f221ec8fcb05ce83db95845700"

# Control message with magic byte
CMWMB="03${CONTROL_MSG}"
CMWMB_LENGTH="59"

{
  echo; echo "Exceptions should not allow new packets to be parsed without starting over"

  {
    echo 8004000311048000002c800006c18000000080000000
    # Wrong length: should be 0x59
    echo 8004010058${CMWMB}
  } | ../apdu.sh

  echo "After exception"

  {

    # Valid second packet, but no first packet.
    echo 80048100${CMWMB_LENGTH}${CMWMB}
  } | ../apdu.sh || echo "PASS"
}
