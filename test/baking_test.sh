#!/usr/bin/env bash
set -Eeuo pipefail

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$DIR"

{
  echo "Authorize baking"
  {
    echo 8001000011048000002c800006c18000000080000000 # Authorize baking
  } | ./apdu.sh
}


{
  echo "Baking a block with multiple packets should fail"

  {
    echo 800681000400000000                           # Reset HWM

    echo 8004000011048000002c800006c18000000080000000
    echo 800401000100  # Prefix packet starting with 00
    echo 800481000a017a06a7700000000102               # Bake block at level 1
  } | ./apdu.sh || true
  echo "Pass"
}

{
  echo "Endorsing a previous level should fail"

  {
    echo 800681000400000000                           # Reset HWM

    echo 8004000011048000002c800006c18000000080000000
    echo 800481000a017a06a7700000000102               # Bake block at level 1

    echo 8004000011048000002c800006c18000000080000000
    echo 800481002a027a06a77000000000000000000000000000000000000000000000000000000000000000000000000001 # Endorse at level 1

    echo 8004000011048000002c800006c18000000080000000
    echo 800481002a027a06a77000000000000000000000000000000000000000000000000000000000000000000000000002 # Endorse at level 2
  } | ./apdu.sh

  {
    echo 8004000011048000002c800006c18000000080000000
    echo 800481002a027a06a77000000000000000000000000000000000000000000000000000000000000000000000000001 # Endorse at level 1 (should fail)
  } | ./apdu.sh || true
  echo "Pass"
}
