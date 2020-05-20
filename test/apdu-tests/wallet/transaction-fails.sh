#!/usr/bin/env bash
set -Eeuo pipefail

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$DIR"

{
  echo; echo "Exceptions should not allow new packets to be parsed without starting over"

  {
    echo 8004000311048000002c800006c18000000080000000
    # Wrong length: should be 0x97
    echo 80040100960316d8aa98f84a30af5871642e9fab07597a14bf0a9a4f37bb8b734fd28007cee10700006fd9ff5e5aad9738883f9d291dd67f888221ad8fea0902904e000050a7e13e2ce14fd935a4258dbff7f4874421981177e083dd7a3a505d16b1e31d0800006fd9ff5e5aad9738883f9d291dd67f888221ad8fa20903bc5000c0c3930700006fd9ff5e5aad9738883f9d291dd67f888221ad8f00
  } | ../apdu.sh

  echo "After exception"

  {

    # Valid second packet, but no first packet.
    echo 80048100970316d8aa98f84a30af5871642e9fab07597a14bf0a9a4f37bb8b734fd28007cee10700006fd9ff5e5aad9738883f9d291dd67f888221ad8fea0902904e000050a7e13e2ce14fd935a4258dbff7f4874421981177e083dd7a3a505d16b1e31d0800006fd9ff5e5aad9738883f9d291dd67f888221ad8fa20903bc5000c0c3930700006fd9ff5e5aad9738883f9d291dd67f888221ad8f00
  } | ../apdu.sh || echo "Pass"
}
