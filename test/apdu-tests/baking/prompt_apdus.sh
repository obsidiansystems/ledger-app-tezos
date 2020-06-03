#!/usr/bin/env bash
set -euo pipefail

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$DIR"

echo "EXPECT: Authorize Baking with Public Key?"
{
    echo 8001000011048000002c800006c18000000080000000 # Authorize baking
} | ../apdu.sh

echo "EXPECT: Provide Public Key"
{
    echo 8003000011048000002c800006c18000000080000000 # Get public key (prompt)
} | ../apdu.sh

echo "No Prompt Expected, BUT SHOULD NOT ERROR"
sleep 3;
{
    echo 8002000011048000002c800006c18000000080000000 # Get public key (no prompt)
} | ../apdu.sh
