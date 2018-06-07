# Ledger-app-tezos

There are two versions of this Ledger Nano S application that are
buildable from this codebase: a baking-only app ("Baking Application"),
and a transaction app that supports everything you might want to use
the Ledger for on Tezos besides baking ("Wallet Application").

## About Key Generation

Every Ledger generates public and private keys for either `ed25519` or
`secp256k1` encryption systems based on a seed (represented by and encoded
in the words associated with that Ledger) and a BIP32 path. The same seed
and BIP32 path will always result in the same key for the same systems.
This means that, to keep your Bitcoin app from knowing your Tezos keys, and vice versa,
different BIP32 paths have to be used for the same Ledger.

All Tezos BIP32 paths begin with `44'/1729'` (the `'` indicates it is
"hardened").  Which Ledger is intended to be used, as well as choice of
encryption system, is indicated by a root key hash, the Tezos-specific
base58 encoding of the hash of the public key at `44'/1729'` on that
Ledger. At the time of this writing, we are currently working on a
command line utility to extract this information from a Ledger.

## The Baking Application

The Baking Application supports 3 operations: Authorize/get public key,
reset high water mark, and sign. It will only sign block headers and
endorsements, as the purpose of the baking app is that it cannot be
co-opted to perform other types of operations. If a Ledger is running
with the Baking Application, it is the expectation of its owner that
no transactions will need to be signed with it.  To sign transactions
with that Ledger, you will need to switch it to using the Wallet Application,
or have the Wallet Application installed on a paired Ledger. Therefore,
if you have a larger stake and bake frequently, we recommend the paired
Ledger approach. If, however, you bake infrequently and can afford to
have your baker offline temporarily, then switching to the Wallet Application
on the same Ledger should suffice.

### Authorize/Get Public Key

The authorize/get public key operation is done with the `tezos-client import` command, e.g.
`tezos-client import secret key my-ledger "ledger://tz1..../44'/1729'"`, where `tz1...` is the
root public key hash for that particular Ledger.

This actually copies only the public
key; it says "import secret key" to indicate that the Ledgers secret key will be considered
available for signing from them on, but it does not actually leave the Ledger.

This sends a BIP32 path to the Ledger, and, assuming the user verifies it,
sends the public key back to the computer and authorizes the appropriate
key for baking. Once a key is authorized for baking, the user will
not have to approve this command again. If a key is not authorized
for baking, signing endorsements and block headers with that key will
be rejected. This authorization data is persisted across runs of the
application.

Only one key can be authorized for baking per Ledger at a time.

### Sign

The sign operation is for signing block headers and endorsements. Block
headers must have monotonically strictly increasing levels; that is, each
block must have a higher level than all previous blocks signed with the
Ledger. This is intended to prevent double baking at the Ledger level, as
a security measure against potential vulnerabilities where the computer
might be tricked into double baking. This feature will hopefully be a
redundant precaution, but it's implemented at the Ledger level because
the point of the Ledger is to not trust the computer. The current High
Water Mark (HWM) -- the highest level to have been baked so far -- is displayed
on the Ledger screen, and is also persisted between runs of the device.

The sign operation will be sent to the Ledger by the baking daemon when configured to bake
with a Ledger key. The Ledger uses the first byte of the information to be signed -- the magic
number -- to tell whether it is a block header (which is verified with the High Water Mark),
an endorsement (which is not), or some other operation (which it will reject).

As long as the key is configured and the high water mark constraint is followed, there is no
user prompting required for signing. The baking app will only ever sign without prompting or
reject an attempt at signing; this operation is designed to be used unsupervised.

### Reset

This feature requires the `ledgerblue` Python package, which is available
at [Blue Loader Python](https://github.com/LedgerHQ/blue-loader-python/).
Please read the instructions at the README there.

There is some possibility that the HWM will need to be reset. This could
be due to the computer somehow being tricked into sending a block with a
high level to the Ledger (either due to an attack or a bug on the computer
side), or because the owner of the Ledger wants to move from the real
Tezos network to a test network, or to a different test network. This
can be accomplished with the reset command.

This is intended as a testing/debug tool and as a possible recovery from
attempted attacks or bugs, not as a day to day command. It requires an
explicit confirmation from the user, and there is no specific utility to
send this command. To send it manually, you can use the following command
line:

`echo 800681000400000000 | python -m Ledgerblue.runScrirpt --apdu`

The last 8 0s indicate the hexadecimal high water mark to reset to. A higher
one can be specified, `00000000` is given as an example as that will allow
all positive block levels to follow.

## The Transaction Application

This application and the Baking Application constitute complementary
apps for different use cases -- which could be on paired Ledgers and
therefore use the same key, or which could also be used in different
scenarios for different accounts.  Baking (determined by magic number)
is rejected by this app.

The "provide address" command on the Wallet Application shows the address the first
time the command is run for any given session. Subsequently, it provides the
address without prompting. To display addresses again, exit the Wallet Application and
restart it. This is again provided for testing/initial set up purposes.

The sign command for the Wallet Application prompts every time for transactions and
other "unsafe" operations, with the generic prompt saying "Sign?" We hope to
eventually display more transaction details along with this. When block headers
and endorsements are sent to the Ledger, they are rejected silently as if the user
rejected them.

## Building and Installation

### Building

If you already have an `app.hex` file or files that you want to use,
e.g. from a release, then you do not need these instructions.  Please skip
to the next section for installation instructions.

You will need to have the BOLOS SDK to use the Makefile, which can be cloned from
[a GitHub repo](https://github.com/LedgerHQ/nanos-secure-sdk). You will also need to
download two compilers:

* [CLANG](http://releases.llvm.org/4.0.0/clang+llvm-4.0.0-x86_64-linux-gnu-ubuntu-16.10.tar.xz)
* [GCC](https://launchpadlibrarian.net/251687888/gcc-arm-none-eabi-5_3-2016q1-20160330-linux.tar.bz2)

The parent directory of both compilers should be in the `BOLOS_ENV` environment variable and the
`BOLOS_SDK`.

To build the baking app, define the environment variable `BAKING_APP=Y`
while using the Makefile. To build the transaction app, leave this
environment variable undefined.

Once we have done more testing, we will also release the `app.hex` files directly.

### Installation

This requires the `ledgerblue` Python package, which is available at
[Blue Loader Python](https://github.com/LedgerHQ/blue-loader-python/).
Please read the instructions at the README there. It will require
you to install some Python dev tools, which it lists, such as
`virtualenv`. Please do so on your platform's native installation system.

To install, use the `install.sh` script, after having built or otherwise
obtained the appropriate `app.hex`.

It takes two parameters. First is the name of the app; we recommend `"Tezos Baking"`
or `"Tezos Wallet"`. Second is the path to the `app.hex` file; if it has built in
place using the `Makefile`, this is optional. An example command line, assuming you are
in the parent directory of the checkout and you have a pre-existing `app.hex` file in your
current directory named `baking_app.hex`, might be:

`ledger-app-tezos/install.sh "Tezos Baking" baking_app.hex`
