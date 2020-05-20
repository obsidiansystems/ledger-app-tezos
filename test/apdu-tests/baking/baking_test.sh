#!/usr/bin/env bash
set -Eeuo pipefail

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$DIR"

fail() {
  echo "$1"
  echo
  exit 1
}


{
  echo; echo "Authorize baking"
  {
    echo 8001000011048000002c800006c18000000080000000 # Authorize baking
  } | ../apdu.sh
}

{
  echo; echo "Self-delegation should work"
  {
    echo 8004000011048000002c800006c18000000080000000
    echo 800481005703cae1b71a3355e4476620d68d40356c5a4e5773d28357fea2833f24cd99c767260a0000aed011841ffbb0bcc3b51c80f2b6c333a1be3df0ec09f9ef01f44e9502ff00aed011841ffbb0bcc3b51c80f2b6c333a1be3df0
  } | ../apdu.sh
}

{
  echo; echo "Baking a block with multiple packets should fail"

  {
    echo 800681000400000000                           # Reset HWM
  } | ../apdu.sh

  echo; echo "Self-delegation and block header"
  ({
    echo 8004000011048000002c800006c18000000080000000
    echo 800401005703cae1b71a3355e4476620d68d40356c5a4e5773d28357fea2833f24cd99c767260a0000aed011841ffbb0bcc3b51c80f2b6c333a1be3df0ec09f9ef01f44e9502ff00aed011841ffbb0bcc3b51c80f2b6c333a1be3df0
    echo 800481000a017a06a7700000000102               # Bake block at level 1
  } | ../apdu.sh && fail ">>> EXPECTED FAILURE") || true


  echo; echo "Prefix 00 byte"
  ({
    echo 8004000011048000002c800006c18000000080000000
    echo 800401000100  # Prefix packet starting with 00
    echo 800481000a017a06a7700000000102               # Bake block at level 1
  } | ../apdu.sh && fail ">>> EXPECTED FAILURE") || true

  echo; echo "Prefix 03"
  ({
    echo 8004000011048000002c800006c18000000080000000
    echo 800401000103  # Prefix packet starting with 03
    echo 800481000a017a06a7700000000102               # Bake block at level 1
  } | ../apdu.sh && fail ">>> EXPECTED FAILURE") || true

  echo; echo "Postfix 00"
  ({
    echo 8004000011048000002c800006c18000000080000000
    echo 800401000a017a06a7700000000102               # Bake block at level 1
    echo 800481000100  # Postfix packet starting with 00
  } | ../apdu.sh && fail ">>> EXPECTED FAILURE") || true
}

{
  echo; echo "Endorsing a previous level should fail"

  {
    echo 800681000400000000                           # Reset HWM

    echo 8004000011048000002c800006c18000000080000000
    echo 800481000a017a06a7700000000102               # Bake block at level 1

    echo 8004000011048000002c800006c18000000080000000
    echo 800481002a027a06a77000000000000000000000000000000000000000000000000000000000000000000000000001 # Endorse at level 1

    echo 8004000011048000002c800006c18000000080000000
    echo 800481002a027a06a77000000000000000000000000000000000000000000000000000000000000000000000000002 # Endorse at level 2
  } | ../apdu.sh

  ({
    echo 8004000011048000002c800006c18000000080000000
    echo 800481002a027a06a77000000000000000000000000000000000000000000000000000000000000000000000000001 # Endorse at level 1 (should fail)
  } | ../apdu.sh && fail ">>> EXPECTED FAILURE") || true
}
