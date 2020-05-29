#!/usr/bin/env bash
set -euo pipefail

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$DIR"

{
    echo 8001000011048000002c800006c18000000080000000 # Authorize baking
    echo 8002000011048000002c800006c18000000080000000 # Get public key (no prompt)
    echo 8003000011048000002c800006c18000000080000000 # Get public key (prompt)
    echo 800A000011048000002c800006c18000000080000000 # Get public key (alternate prompt)
} | ../apdu.sh
