# To bump:
#   1. Select channel from: http://howoldis.herokuapp.com/
#   2. Copy the URL to a `nixexprs.tar.xz` file. It should include hashes (i.e. not be a redirect).
#   3. Run `nix-prefetch-url --unpack <url>` to get the SHA256 hash of the contents.*
#   4. Update the URL and SHA256 values below.

import (builtins.fetchTarball {
  url = "https://releases.nixos.org/nixos/unstable/nixos-19.09pre174426.acbdaa569f4/nixexprs.tar.xz";
  sha256 = "1r5vk4z90wzh9k7xyib0zr9r6nssbw9fady620ma9cima1ixzyqj";
})
