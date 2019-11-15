#!/usr/bin/env bash
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$DIR"

die() {
    2>&1 echo "$@"
    exit 1
}

# get version first to make sure ledger is available
echo 8000000000 | ./apdu.sh > /dev/null || die "Ledger is not connected, please reconnect it and rerun this script."

echo Running named delegates test...
printf '%*s\n' "${COLUMNS:-$(tput cols)}" '' | tr ' ' -
cat <<EOF
UNNAMED DELEGATES

Please verify that the following fields appear on the ledger:

CONFIRM DELEGATION
  Delegate: tz1eY5Aqa1kXDFoiebL28emyXFoneAoVg1zh
  Delegate name: Custom Delegate: please verify the address

afterwards you can accept this delegation.
EOF
{
    echo 8004000311048000002c800006c18000000080000000
    echo 8004810053034376b9304606f1dc37a507b7d2e730e60a3040389f57d2ccc3cf2520607c52d66e00cf49f66b9ea137e11818f2a78b4b6fc9895b4e50e80904f44e00ff00cf49f66b9ea137e11818f2a78b4b6fc9895b4e50
} | ./apdu.sh > /dev/null
printf '%*s\n' "${COLUMNS:-$(tput cols)}" '' | tr ' ' -
cat <<EOF
NAMED DELEGATES

Please verify that the following fields appear on the ledger:

CONFIRM DELEGATION
  Delegate: tz1TDSmoZXwVevLTEvKCTHWpomG76oC9S2fJ
  Delegate name: Tezos Capital Legacy

afterwards you can accept this delegation.
EOF
{
    echo 8004000311048000002c800006c18000000080000000
    echo 8004810056035379ba9122785f71ec283a3b6398c05edc8f8d77eef885d167d22c50e9f9c26c6e00cf49f66b9ea137e11818f2a78b4b6fc9895b4e50019e1480ea30e0d403ff00531ab5764a29f77c5d40b80a5da45c84468f08a1
} | ./apdu.sh > /dev/null

echo Name delegate test suite is finished.
