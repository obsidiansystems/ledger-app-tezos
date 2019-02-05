# To bump:
#   1. Select channel from: http://howoldis.herokuapp.com/
#   2. Copy the URL to a `nixexprs.tar.xz` file. It should include hashes (i.e. not be a redirect).
#   3. Run `nix-prefetch-url --unpack <url>` to get the SHA256 hash of the contents.*
#   4. Update the URL and SHA256 values below.

import (builtins.fetchTarball {
  url = "https://releases.nixos.org/nixpkgs/nixpkgs-19.03pre167327.11cf7d6e1ff/nixexprs.tar.xz";
  sha256 = "0y0fs0j6pb9p9hzv1zagcavvpv50z2anqnbri6kq5iy1j4yqaric";
})
