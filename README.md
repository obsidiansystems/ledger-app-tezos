# ledger-app-tezos

There are two versions of this ledger app that are buildable from this
codebase: a baking-only app ("the baking app"), and a transaction app
that supports everything you might want to use the ledger for on Tezos
besides baking ("the transaction app"). To build the baking app, define
the environment variable `BAKING_APP=Y` while using the Makefile. To
build the transaction app, leave this environment variable undefined.

## The Baking App

The baking app supports 3 operations: Authorize/get public key, reset high water
mark, and sign. It will only sign block headers and endorsements, as the
purpose of the baking app is that it cannot be coopted to perform other
types of operations. If a ledger is running with the baking app, it is the
expectation of its owner that no transactions will need to be signed with it.
To sign transactions with that ledger, you will need to switch it to using the
general app, or have the general app installed on a paired ledger. We recommend
the paired ledger approach.

### Authorize/Get Public Key

The authorize/get public key operation is done with the `tezos-client import` command, e.g.
`tezos-client import ledger secret key my-ledger ed25519`. This actually copies only the public
key; it says "import secret key" to indicate that the ledgers secret key will be considered
available for signing from them on, but it does not actually leave the ledger.

This sends a BIP32 path to the ledger, and, assuming the user verifies it, sends the
public key back to the computer and authorizes the appropriate key for baking. Once a key
is authorized for baking, the user will not have to approve this command again. If a key
is not authorized for baking, endorsements and block headers with that key will be rejected
for signing. This authorization data is persisted across runs of the application.

For now, only one key can be authorized for baking per ledger at a time.

### Sign

The sign operation is for signing block headers and endorsements. Block
headers must have monotonically strictly increasing levels; that is, each
block must have a higher level than all previous blocks signed with the
ledger. This is intended to prevent double baking at the ledger level, as
a security measure against potential vulnerabilities where the computer
might be tricked into double baking. This feature will hopefully be a
redundant precaution, but it's implemented at the ledger level because
the point of the ledger is to not trust the computer. The current high
water mark -- the highest level to have been baked so far -- is displayed
on the ledger screen, and is also persisted between runs of the device.

The sign operation will be sent to the ledger by the baking daemon when configured to bake
with a ledger key. The ledger uses the first byte of the information to be signed -- the magic
number -- to tell whether it is a block header (which is verified with the high water mark),
an endorsement (which is not), or some other operation (which it will reject).

As long as the key is configured and the high water mark constraint is followed, there is no
user prompting required for signing. The baking app will only ever sign without prompting or
reject an attempt at signing; this operation is designed to be used unsupervised.

### Reset

There is some possibility that the high water mark will need to be
reset. This could be due to the computer somehow being tricked into
sending a block with a high level to the ledger (either due to an attack
or a bug on the computer side), or because the owner of the ledger wants
to move from the real Tezos network to a test network, or to a different
test network. This can be accomplished with the reset command.

This is intended as a testing/debug tool and as a possible recovery from
attempted attacks or bugs, not as a day to day command. It requires an
explicit confirmation from the user, and there is no specific utility to
send this command. To send it manually, you can use the following command
line (assuming you have the `ledgerblue` Python package installed):

`echo 800681000400000000 | python -m ledgerblue.runScrirpt --apdu`

The last 8 0s indicate the hexadecimal high water mark to reset to. A higher
one can be specified, `00000000` is given as an example as that will allow
all positive block levels to follow.

## The Transaction App

This app and the baking app constitute complementary apps for different
use cases -- which could be on paired ledgers and therefore use the same key,
or which could also be used in different scenarios for different accounts.
Baking (determined by magic number) is rejected by this app.

The "provide address" command on the general app shows the address the first
time the command is run for any given session. Subsequently, it provides the
address without prompting. To display addresses again, exit the ledger app and
restart it. This is again provided for testing/initial set up purposes.

The sign command for the general app prompts every time for transactions and
other "unsafe" operations, with the generic prompt saying "Sign?" We hope to
eventually display more transaction details along with this. When block headers
and endorsements are sent to the ledger, they are rejected as if the user
rejected them.
