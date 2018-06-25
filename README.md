# Ledger-app-tezos

This repository contains two [Tezos](https://www.tezos.com/)-related
applications that can be loaded onto a
[Ledger Nano S](https://www.ledgerwallet.com/products/ledger-nano-s):

  1. The "Tezo Bake" application is for baking Tezos (signing and publishing
     new blocks).
  2. The "Tezos Sign" application is for making XTZ transactions, and
     everything you might want to use the Ledger for on Tezos besides baking.

To use these apps, you must be sure to have up-to-date firmware on the ledger.
This code was tested with version 1.4.2. Please use the Ledger's tools,
including their Chrome plugin, to do this.

## Linux only: Permissioning USB Access

You need to set `udev` rules to set the permissions so that your user account
can access the Ledger device. This requires system administration privileges
on your Linux system.

### NixOS Instructions

For NixOS, you can set the udev rules by adding the following to the NixOS
configuration file typically located at `/etc/nixos/configuration.nix`:

```nix
services.udev.extraRules = ''
  SUBSYSTEMS=="usb", ATTRS{idVendor}=="2581", ATTRS{idProduct}=="1b7c", MODE="0660", GROUP="users"
  SUBSYSTEMS=="usb", ATTRS{idVendor}=="2581", ATTRS{idProduct}=="2b7c", MODE="0660", GROUP="users"
  SUBSYSTEMS=="usb", ATTRS{idVendor}=="2581", ATTRS{idProduct}=="3b7c", MODE="0660", GROUP="users"
  SUBSYSTEMS=="usb", ATTRS{idVendor}=="2581", ATTRS{idProduct}=="4b7c", MODE="0660", GROUP="users"
  SUBSYSTEMS=="usb", ATTRS{idVendor}=="2581", ATTRS{idProduct}=="1807", MODE="0660", GROUP="users"
  SUBSYSTEMS=="usb", ATTRS{idVendor}=="2581", ATTRS{idProduct}=="1808", MODE="0660", GROUP="users"
  SUBSYSTEMS=="usb", ATTRS{idVendor}=="2c97", ATTRS{idProduct}=="0000", MODE="0660", GROUP="users"
  SUBSYSTEMS=="usb", ATTRS{idVendor}=="2c97", ATTRS{idProduct}=="0001", MODE="0660", GROUP="users"
'';
```

Depending on your system's settings, you may wish to replace `users` with another group. Everyone
in that group will get permissions for accessing the ledger.

Once you have added this, run `sudo nixos-rebuild switch` to activate the configuration,
and unplug your Ledger and plug it in again for the changes to take effect.

### Instructions for Other Distros
For non-Nix users, a
[script](https://raw.githubusercontent.com/LedgerHQ/udev-rules/master/add_udev_rules.sh)
is available from LedgerHQ for this purpose. Download this script, read it, and run it as root.
You might have to adjust which group this script uses, and modify the script for that purpose.
I recommend against just piping `wget` into `sudo bash` without adjusting the script to your
computer's configuration.

Subsequently, unplug your ledger, and plug it in again for the changes to take effect.

## Nix Only: Setting up the Tezos Baking Platform

These instructions work with NixOS, or they also work if you install Nix on another
Linux distribution.

Please clone the
[Tezos Baking Platform](https://gitlab.com/obsidian.systems/tezos-baking-platform) and
check out the `zeronet-new` branch, updating submodules. These are the commands to run
in the freshly-cloned repo:

```
git checkout zeronet-new
git submodule sync
git submodule update --recursive --init
```

At that point, this repo will be checked out in `tezos-baking-platform/ledger/ledger-app`.
`tezos-baking-platform/ledger` will contain easy installation and building scripts,

## Building and Installing the Ledger App

### Nix Instructions

Make sure you've followed the Nix set up instructions above.  Make sure
your Ledger is plugged in and on the app selection screen.

Then run `tezos-baking-platform/ledger/setup_ledger.sh`, which will install both
apps on your ledger, prompting you with authorization.

The first time you run this, you will be prompted to download some
large files and manually add them to your Nix store. This is due to a
known limitation of Nix; it is not a fatal error message, just a little
additional work. Follow the instructions from the error message.

You can also install `wallet.hex` and `baking.hex` files you get as part of a release,
which process will be described below.

### Advanced Linux: Building Your Own `*.hex` Files without Nix

This is done from this repo. It may be easier to use released `*.hex` files.

You will need to have the BOLOS SDK to use the Makefile, which can be cloned from
[a GitHub repo](https://github.com/LedgerHQ/nanos-secure-sdk). You will also need to
download two compilers for use with the SDK. Note that these are specialized compilers
to cross-compile for the ARM-based platform of the Ledger; please don't use the versions of
`gcc` and `clang` that come with your system.

* [CLANG](http://releases.llvm.org/4.0.0/clang+llvm-4.0.0-x86_64-linux-gnu-ubuntu-16.10.tar.xz)
* [GCC](https://launchpadlibrarian.net/251687888/gcc-arm-none-eabi-5_3-2016q1-20160330-linux.tar.bz2)

The parent directory of both compilers should be in the `BOLOS_ENV` environment variable and the
`BOLOS_SDK`.

To build the baking app, define the environment variable `BAKING_APP=Y`
while using the Makefile. To build the transaction app, leave this
environment variable undefined:


```
BAKING_APP=Y make
mv bin/app.hex baking.hex
make clean
BAKING_APP= make
mv bin/app.hex wallet.hex
```

### Non-Nix (incl. macOS): Installing `*.hex` Files on the Ledger

On systems with Nix set up, you can use the `setup_ledger.sh` script
from Tezos Baking Platform, or you can follow these instructions as well.

Ledger's primary interface for loading an app onto a Ledger device is a Chrome
app called [Ledger Manager][ledger-manager]. However, it only supports a limited
set of officially-supported apps like Bitcoin and Ethereum, and Tezos is not yet
among these. So you will need to use a command-line tool called the [BOLOS
Python Loader][bolos-python-loader].

#### Installing BOLOS Python Loader

Install `libusb` and `libudev`, with the relevant headers. On Debian-based
distros, including Ubuntu, the packages with the headers are suffixed with `-dev`.
Other distros will have their own conventions. So, for example, on Ubuntu, you can do this with:

```
sudo apt-get install libusb-1.0.0-dev libudev-dev # Ubuntu example
```

Then, install `pip3`. You must install `pip3` for this and not `pip`. On Ubuntu:

```
sudo apt-get install python3-pip # Ubuntu example
```

Now, on any operating system, install `virtualenv` using `pip3`. It is important to use
`pip3` and not `pip` for this, as this module requires `python3` support.

```
sudo pip3 install virtualenv # Any OS
```

Then create a Python virtual environment (abbreviated *virtualenv*). You could
call it anything, but we shall call it "ledger". This will create a directory
called "ledger" containing the virtualenv. Then, you must enter the `virtualenv`. If you
do not successfully enter the `virtualenv`, future commands will fail. You can tell
you have entered the virtualenv when your prompt is prefixed with `(ledger)`.

```
virtualenv ledger # Any OS
source ledger/bin/activate
```

Your terminal session -- and only that terminal session -- will now be in the virtual env.
To have a new terminal session enter the virtualenv, run the above command only in that
terminal session.

#### ledgerblue

Within the virtualenv environment -- making sure
that `(ledger)` is showing up before your prompt -- use pip to install the `ledgerblue` [Python
package][pypi-ledgerblue]. This will install the Ledger Python packages into the
virtualenv; they will be available only in a shell where the virtualenv has been
activated.

```
pip install ledgerblue
```

If you have to use `sudo` or `pip3` here, that is an indication that you have not correctly
set up `virtualenv`. It will still work in such a situation, but please research other material
on troubleshooting `virtualenv` set-up.

#### Load the app onto the Ledger device

Next you'll use the installation script to install the app on your Ledger Nano.
Make sure your Ledger is plugged into your computer. Enter your PIN but do not
enter any apps that might already be installed on your Ledger. Also be sure that
your Ledger has not gone to sleep (with *vires in numeris* scrolling across the
screen); if you are already in an app or the Ledger is asleep, your installation
process will fail.

We recommend staying at your computer and keeping an eye on the Ledger's screen
as you continue. You may want to read the rest of these instructions before you
begin installing, as you will need to confirm and verify a few things during the
process.

Still within the virtualenv, run the `./install.sh` command.  This script
is in the root directory of this very repo. This script takes two
parameters, the first of which is the *name* of the application you are
installing, and the second is the path to the `app.hex` file:

* If you are installing the baking app, we recommend using the name "Tezos
  Baking".

  ```
  ./install.sh "Tezos Baking" baking.hex
  ```

* If you are installing the transaction app, we recommend using the name "Tezos
  Wallet".

  ```
  ./install.sh "Tezos Wallet" wallet.hex
  ```

The first thing that should come up in your terminal is a message that looks
like this:

```
Generated random root public key : <long string of digits and letters>
```

Look at your Ledger screen and verify that the digits of that key match the
digits you can see on your Ledger screen. What you see on your Ledger screen
should be just the beginning and ending few characters of the longer string that
printed in your terminal.

You will need to push confirmation buttons on your Ledger Nano a few times
during the installation process and re-enter your PIN code near the end of the
process. You should finally see the Tezos logo appear on the screen.

If you see the "Generated random root public key" message and then something
that looks like this:

```
Traceback (most recent call last):
File "/nix/store/96wn2gz3mwi71gwcrvpfg39bsymd7gqx-python3-3.6.5/lib/python3.6/runpy.py", line 193, in _run_module_as_main
<...more file names...>
OSError: open failed
```

the most likely cause is that your `udev` rules are not set up correctly, or you
did not unplug your Ledger between setting up the rules and attempting to
install. Please confirm the correctness of your `udev` rules.

To load a new version of the Tezos application onto the Ledger in the future,
you can run the command again, and it will automatically remove any
previously-loaded version.

#### Removing Your App

If you'd like to remove your app, you can do this. In the virtualenv
described in the last sections, run this command:

```
python \
  -m ledgerblue.deleteApp \
  --targetId 0x31100003 \
  --appName "Tezos"
```

Replace the `appName` parameter "Tezos" with whatever app name you used when you
loaded the app onto the device.

Then follow the prompts on the Ledger screen.

#### Confirming the Installation Worked

You should now have two apps, `Tezos Baking` and `Tezos Wallet`. The `Tezos Baking` app
should display also a `0` on the screen, which is the highest block level baked so far (`0`
in case of no blocks). The `Tezos Wallet` app will just display `Tezos`.

## Nix Only: Building `tezos-client`

`tezos-baking-platform` contains a `default.nix` file that should build Tezos tools,
for the version of `tezos` that `zeronet-new` points to as a submodule.
Please use `nix-build` or `nix-shell` to build them.

Building Tezos on non-Nix platform is outside of the scope of this document.

## Background on the Ledger: Key Generation

Every Ledger generates public and private keys for either `ed25519` or
`secp256k1` encryption systems based on a seed (represented by and encoded
in the words associated with that Ledger) and a BIP32 path. `secp256r1` will
also soon be supported, but is not completely supported yet.

The same seed and BIP32 path will always result in the same key for the
same systems.  This means that, to keep your Bitcoin app from knowing
your Tezos keys, and vice versa, different BIP32 paths have to be used
for the same Ledger. This also means that, in order to sync two ledgers,
you can set them to the same seed, represented as 24 or some other number
of natural language words (English by default).

All Tezos BIP32 paths begin with `44'/1729'` (the `'` indicates it is
"hardened").  Which Ledger is intended to be used, as well as choice of
encryption system, is indicated by a root key hash, the Tezos-specific
base58 encoding of the hash of the public key at `44'/1729'` on that
Ledger. Because all Tezos paths start with this, in `tezos-client` commands
it is implicit. To extract the root key hash, please run, with a Tezos app open
on your Ledger:

`tezos-client list connected ledgers`

This will give separate root key hashes for each encryption system
(either `ed25519` or `secp256k1`. The Ledger does not currently support
non-hardened path components. All components of all paths must be
hardened, which is indicated by following them with a `'` character. This
character may need to be escaped from the shell through backslashes `\`
or double-quotes `"`.

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

### Get Public Key

The "get public key" operation is done with the `tezos-client
import` command, e.g.  `tezos-client import secret key my-ledger
"ledger://tz1..../0'/0'"`, where `tz1...` is the root public key hash
for that particular Ledger.

This actually copies only the public key; it says "import secret key"
to indicate that the Ledgers secret key will be considered available
for signing from them on, but it does not actually leave the Ledger.

This sends a BIP32 path to the Ledger, and, assuming the user verifies
it, sends the public key back to the computer.

### Authorize Public Key

You need to run a specific command to authorize a key for baking. Once
a key is authorized for baking, the user will not have to approve this
command again. If a key is not authorized for baking, signing endorsements
and block headers with that key will be rejected. This authorization
data is persisted across runs of the application.  Only one key can be
authorized for baking per Ledger at a time.

You need either the `virtualenv` installed above (non-Nix) or the
`tezos-baking-platform` (Nix).

Assuming you want `44'/1729'/0'/0'` If you are using `virtualenv`, run:

```
echo 8001000011048000002c800006c18000000080000000 | python -m ledgerblue.runScript --apdu
```

If you are using `tezos-baking-platform`, there is a script `apdu.sh` in the `ledger`
directory. Run:

```
echo 8001000011048000002c800006c18000000080000000 | ./apdu.sh
```

That string of hex digits can be customized for other key paths. The format is this:

```
800100 -- invariant prefix indicating our intention to authorize this key.
00     -- indicates signing type, use 00 for ed25519 or 01 for secp256k1
11     -- indicates length of rest of the string: hex(number of path components * 4 + 1)
04     -- indicates number of path components
8000002c -- This is 44 (the first path component) in hex, with the upper bit set to 1 to indicate hardened
800006c1 -- This is 1729 in hex with the upper bit again set to 1
80000000 -- Represents 0' in the same way
80000000 -- 4th path component
```

A tool will soon be made available to do this automatically.

To confirm that this worked, you can send a fake block to the ledger to sign.

```
printf '%s\n%s\n' "8004000011048000002c800006c18000000080000000" "8004810006010000000101" | ./apdu.sh
```

Or:

```
printf '%s\n%s\n' "8004000011048000002c800006c18000000080000000" "8004810006010000000101" | python -m ledgerblue.runScript --apdu
```

The first line is similar to the above format, with the prefix as `800400` instead of `800100`.
The second line is simply a fake block header with a level of 1, which the Ledger app will think
is a block header and display a new high water mark of `1`, thereby confirming that the Ledger works.

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

The sign operation will be sent to the Ledger by the baking daemon when
configured to bake with a Ledger key. The Ledger uses the first byte
of the information to be signed -- the magic number -- to tell whether
it is a block header (which is verified with the High Water Mark),
an endorsement (which is not), or some other operation (which it will
reject).

As long as the key is configured and the high water mark constraint is
followed, there is no user prompting required for signing. The baking app
will only ever sign without prompting or reject an attempt at signing;
this operation is designed to be used unsupervised.

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

There is a script for Nix users in `tezos-baking-platform` called `reset.sh` which
does the same thing.

### Practical Baking

In practice, when you bake, you will first have to delegate the ledger account to itself.
To do this, use the Wallet App, not the Baking App, and then switch back to the Baking App.

## Transactions: The Wallet Application

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
