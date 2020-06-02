#!/usr/bin/env bash
set -Eeuo pipefail

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$DIR"

# tezos-client transfer 1 from <my-ledger> to <other tz1 account> --burn-cap 0.257
# fee = 0.001283
# storage limit = 0
CONTROL_MSG="17777d8de5596705f1cb35b0247b9605a7c93a7ed5c0caa454d4f4ff39eb411d6c00cf49f66b9ea137e11818f2a78b4b6fc9895b4e50830ae58003c35000c0843d0000eac6c762212c4110f221ec8fcb05ce83db95845700"
CONTROL_LENGTH="58"

# Control message with magic byte
CMWMB="03${CONTROL_MSG}"
CMWMB_LENGTH="59"

{
  echo; echo "Messages with 129 prefix bytes should be Sign Unverified (ACCEPT THIS)"
  echo "MUST BE: Unrecognized Operation: Sign Hash"

  {
    echo 8004000011048000002c800006c18000000080000000
    # 128 bytes
    echo 80040100800300000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
    echo 800401000103 # magic byte
    echo 80048100${CONTROL_LENGTH}${CONTROL_MSG}
  } | ../apdu.sh
}


{
  echo; echo "Prefixed packets should cause Sign Unverified (ACCEPT THIS)"
  echo "MUST BE: Unrecognized Operation: Sign Hash"

  {
    echo 8004000311048000002c800006c18000000080000000
    echo 800401000103
    echo 80048100${CMWMB_LENGTH}${CMWMB}
  } | ../apdu.sh
}

{
  echo; echo "Trailing packets should cause Sign Unverified (ACCEPT THIS)"
  echo "MUST BE: Unrecognized Operation: Sign Hash"

  {
    echo 8004000311048000002c800006c18000000080000000
    echo 80040100${CMWMB_LENGTH}${CMWMB}
    echo 800481000103
  } | ../apdu.sh
}

{
  echo; echo "Prefixed and trailing packets should cause Sign Unverified (ACCEPT THIS)"
  echo "MUST BE: Unrecognized Operation: Sign Hash"

  {
    echo 8004000311048000002c800006c18000000080000000
    echo 800401000103
    echo 80040100${CMWMB_LENGTH}${CMWMB}
    echo 800481000103
  } | ../apdu.sh
}

{
  echo; echo "Two valid packets should still cause Sign Unverified (ACCEPT THIS)"
  echo "MUST BE: Unrecognized Operation: Sign Hash"

  {
    echo 8004000311048000002c800006c18000000080000000
    echo 80040100${CMWMB_LENGTH}${CMWMB}
    echo 80048100${CMWMB_LENGTH}${CMWMB}
  } | ../apdu.sh
}

{
  echo; echo "Valid transaction should be parsed (SHOULD BE: Transfer 1 ... ACCEPT THIS)"
  echo "MUST BE: Confirm Transaction"

  {
    echo 8004000311048000002c800006c18000000080000000
    echo 80048100${CMWMB_LENGTH}${CMWMB}
  } | ../apdu.sh
}
