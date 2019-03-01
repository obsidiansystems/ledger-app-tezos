# Tezos Ledger Nano S Applications

## Overview

Whether you're baking or just trading XTZ, you want to store your keys securely.
This is what a "hardware wallet" like the
[Ledger Nano S](https://www.ledgerwallet.com/products/ledger-nano-s) is for.
Your private keys never leave the device, and it performs the signing
operations. To use a Ledger Nano S with [Tezos](https://www.tezos.com/), you need to
load Tezos-specific software onto it.

The term "hardware wallet" can refer to several devices that store your private
keys in a secure way. The term "wallet" refers to the fact that it stores your
"money" -- in the case of Tezos, it stores your tez. Remember, storing your
tokens means storing the private keys that control your tokens. But the wallet
also has other uses, including an app that helps you securely and easily
interact with the network, including creating Tezos transactions and baking
blocks.

This repository contains two Ledger Nano S applications:

  1. The "Tezos Baking" application is for baking: signing new blocks and
     endorsements. For more information about baking, see
     [*Benefits and Risks of Home Baking*](https://medium.com/@tezos_91823/benefits-and-risks-of-home-baking-a631c9ca745).
  2. The "Tezos Wallet" application is for making XTZ transactions, originating contracts, delegation, and voting. Basically
     everything you might want to use the Ledger Nano S for on Tezos besides baking.

It is possible to do all of these things without a hardware wallet, but using a
hardware wallet provides you better security against key theft.

This documentation was originally written when there was no GUI support, so everything is
tailored towards the command line.  We recommend you read this entire
document to understand the commands available, and which commands are
most appropriate to your situation. This will require judgment on how
best to meet your needs, and this document will also provide context to
help you understand that.

This document is not a comprehensive guide to setting up Tezos
software. While it covers some aspects of setting up and installing
Tezos nodes and clients, especially as it interacts with the Ledger Nano S,
you should familiarize yourself with the [Tezos Documentation](https://tezos.gitlab.io/master/) and community resources such as Tezos Community's guide on [building a node](https://github.com/tezoscommunity/FAQ/blob/master/Compile_Mainnet.md). If you have questions, please ask them on the [Tezos Stack Exchange](https://tezos.stackexchange.com/).

This document is also not a guide on how to use Linux. It assumes you
know how to install and configure a Linux system to your general needs,
use the command line, or configure GitHub access. Occasionally, it will
recommend things like editing a script to match your configuration.
Learning how to run commands on Linux, edit scripts, or configure your
user accounts to enable groups, is outside the scope of this document,
but resources for all of those things are available on the Internet.

The commands in these instructions have only been tested on Linux. If
you use any form of virtualization, e.g. docker or VirtualBox, please
consult the documentation of that virtualization system to determine
how to access USB from inside the virtualization, as that can be a
complicated and difficult process.

The commands in this document have been tested as is, and have the correct
privileges. If you find yourself using `sudo` to run commands that are not
listed as requiring `sudo`, that likely indicates a problem with your
configuration, most often the `udev` configuration. Using `sudo` for commands
that should not require it can create security vulnerabilities and corrupt
your configuration.

## Set up your Ledger Nano S device

Tezos recommends a hardware wallet called the Ledger Nano S. When you first get
it and set it up, part of the setup process is generating a keypair. The keypair
will be associated with a rather long seed phrase that you must write down and
keep securely. We'll discuss that seed phrase more below. You also set a PIN
code that allows you to unlock the device so that it will sign messages. You can
then install the Tezos app to use the Ledger Nano S to interact directly with the
Tezos network (see more about this in the app instructions, forthcoming).
However, your Ledger Nano S will ask for confirmation before it sends your keys to sign
transactions or blocks, and you must confirm by physically pushing a button on
the device, and that provides some security against an attacker taking control
over your keys.

### Protecting your key

The seed phrase is an encoding of your private key itself and can be used to
restore your key. If you lose your Ledger device or destroy it somehow, you can buy a
new one and set it up with the seed phrase from your old one, hence restoring
your tokens.

Consequently, is is extremely important that you keep your seed phrase written
down somewhere safe. Losing it can mean you lose control of your account should
you, for example, lose your Ledger device. Keeping it somewhere a hacker could find it
(such as in a file on your internet-connected computer) means your private key
can fall into the wrong hands.

You will write it down on paper, along with your PIN, and store it. If you will
have a large amount of money, consider putting your paper in a safe or safe
deposit box, but at the very least keep it away from places where children,
dogs, housekeepers, or obnoxious neighbors could inadvertently destroy it.

### Protecting Your Key -- Further Advanced Reading

More advanced techniques for those interested in even more layers of security
or plausible deniability features should look at
[Ledger's documentation on this](https://support.ledgerwallet.com/hc/en-us/articles/115005214529-Advanced-Passphrase-options).

Note that Ledger devices with different seeds will appear to `tezos-client` to be
different hardware wallets. Note also that it can change what key is authorized in
Tezos Baking. When using these features in a Ledger hardware wallet used for baking,
please exit and re-start Tezos Baking right before baking is supposed to
happen, and manually verify that it displays the key you expect to bake for.

Tezos Wallet does not require such extra steps, and so these extra
protections are more appropriate for keys used for transaction than
they are for keys used for baking. If you do use these features, one
technique is that your tez be stored in the passphrase-protected and
deniable account, and that you delegate them to a baking account. This
way, the baking account won't actually store the vast majority of the tez.

### Ledger Nano S firmware update

To use these apps, you must be sure to have [up-to-date
firmware](https://support.ledgerwallet.com/hc/en-us/articles/360002731113)
on the Ledger device. This code was tested with version
1.5.5. Please use [Ledger Live](https://www.ledger.com/pages/ledger-live) to do this.

### udev rules (Linux only)

You need to set `udev` rules to set the permissions so that your user account
can access the Ledger device. This requires system administration privileges
on your Linux system.

#### Instructions for most distros (not including NixOS)

LedgerHQ provides a
[script](https://raw.githubusercontent.com/LedgerHQ/udev-rules/master/add_udev_rules.sh)
for this purpose. Download this script, read it, customize it, and run it as root:

```
$ wget https://raw.githubusercontent.com/LedgerHQ/udev-rules/master/add_udev_rules.sh
$ chmod +x add_udev_rules.sh
```

At this point, please use your favorite editor to modify
`add_udev_rules.sh` to your configuration, e.g., by replacing `plugdev` with an
appropriate group for your system configuration. We recommend against
running the next command without reviewing the script and modifying it
to match your configuration.

```
$ sudo ./add_udev_rules.sh
```

Subsequently, unplug your ledger hardware wallet, and plug it in again for the changes to take
effect.

#### Instructions for NixOS

For NixOS, you can set the udev rules by adding the following to the NixOS
configuration file typically located at `/etc/nixos/configuration.nix`:

```nix
{
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
}
```

Depending on your system's settings, you may wish to replace `users` with
another group. Everyone in that group will get permissions for accessing the
ledger Nano S.

Once you have added this, run `sudo nixos-rebuild switch` to activate the
configuration, and unplug your Ledger device and plug it in again for the changes to
take effect.

## Installing the Applications with Ledger Live

The easiest way to obtain and install the Tezos Ledger Nano S apps is to download them
from [Ledger Live](https://www.ledger.com/pages/ledger-live). Tezos Wallet is readily available 
in Ledger Live's Manager. To download Tezos Baking, you'll need to enable 'Developer Mode' in Settings.

If you've used Ledger Live for app installation, you can skip ahead to [Registering the Ledger Nano S with the node](#registering-the-ledger-nano-s-with-the-node).

## Obtaining the Ledger Nano S apps without Ledger Live

If you are using the [Nix package manager](https://nixos.org/nix/), you can skip
this section and the next one; go directly to
[Tezos Baking Platform](https://gitlab.com/obsidian.systems/tezos-baking-platform)
for simpler Nix-based installation, where documentation should be in `ledger/README.md`.
Then return to this document and continue reading at *Using the Ledger Nano S apps*.

The second easiest way to obtain both applications (after Ledger Live) is to download `.hex` files
from the [releases](https://github.com/obsidiansystems/ledger-app-tezos/releases)
page of this repo. After doing so, skip ahead to *Installing the apps onto your Ledger device*.
You will need to expand the releases tarball somewhere and copy the
baking.hex and wallet.hex files into the ledger-app-tezos directory.
If you want to compile the applications yourself, keep reading this section.

### Compiling the `.hex` files

The first thing you'll need to do is clone this repository:

```
$ git clone https://github.com/obsidiansystems/ledger-app-tezos.git
```

You will need to have the
[BOLOS SDK](http://ledger.readthedocs.io/en/latest/userspace/getting_started.html)
to use the Makefile, which can be cloned from Ledger's
[nanos-secure-sdk](https://github.com/LedgerHQ/nanos-secure-sdk) git repository.
You will also need to download two compilers for use with the SDK.
Note that these are specialized compilers to cross-compile for the ARM-based
platform of the Ledger device; please don't use the versions of `clang` and `gcc` that
come with your system.

  * [CLANG](http://releases.llvm.org/4.0.0/clang+llvm-4.0.0-x86_64-linux-gnu-ubuntu-16.10.tar.xz)
  * [GCC](https://launchpadlibrarian.net/251687888/gcc-arm-none-eabi-5_3-2016q1-20160330-linux.tar.bz2)

All of the environment setup can be accomplished with the following commands.

Obtain the BOLOS SDK and the compilers it needs:

```
$ git clone https://github.com/LedgerHQ/nanos-secure-sdk
$ wget -O clang.tar.xz http://releases.llvm.org/4.0.0/clang+llvm-4.0.0-x86_64-linux-gnu-ubuntu-16.10.tar.xz
$ wget -O gcc.tar.bz2 https://launchpadlibrarian.net/251687888/gcc-arm-none-eabi-5_3-2016q1-20160330-linux.tar.bz2
```

Unzip the compilers and move them to appropriately-named directories:

```
$ mkdir bolos_env
$ tar -xJf clang.tar.xz --directory bolos_env
$ mv bolos_env/clang+llvm-4.0.0-x86_64-linux-gnu-ubuntu-16.10 bolos_env/clang-arm-fropi
$ tar -xjf gcc.tar.bz2 --directory bolos_env
```

Set environment variables:

```
$ export BOLOS_SDK=$PWD/nanos-secure-sdk
$ export BOLOS_ENV=$PWD/bolos_env
```

To build the Tezos Wallet app:

```
$ APP=tezos_wallet make
$ mv bin/app.hex wallet.hex
```
If this results in an error message that includes this line (possibly repeatedly):

```
#include <bits/libc-header-start.h>
```
you may need to run:

```
$ sudo apt-get install libc6-dev gcc-multilib g++-multilib
```
and then re-run the `make` command.

Note that if you build *both* apps, you need to run `make clean` before building
the second one. So, to build both apps run:

```
$ APP=tezos_wallet make
$ mv bin/app.hex wallet.hex
$ make clean
$ APP=tezos_baking make
$ mv bin/app.hex baking.hex
```

To build just the Tezos Baking App:

```
$ APP=tezos_baking make
$ mv bin/app.hex baking.hex
```

### Installing the apps onto your Ledger device without Ledger Live

Manually installing the apps requires a command-line tool called the
[BOLOS Python Loader](https://ledger.readthedocs.io/projects/blue-loader-python/en/0.1.16/index.html).

### Installing BOLOS Python Loader

Install `libusb` and `libudev`, with the relevant headers. On Debian-based
distros, including Ubuntu, the packages with the headers are suffixed with
`-dev`. Other distros will have their own conventions. So, for example, on
Ubuntu, you can do this with:

```
$ sudo apt-get install libusb-1.0.0-dev libudev-dev # Ubuntu example
```

Then, install `pip3`. You must install `pip3` for this and not `pip`. On Ubuntu:

```
$ sudo apt-get install python3-pip # Ubuntu example
```

Now, on any operating system, install `virtualenv` using `pip3`. It is important
to use `pip3` and not `pip` for this, as this module requires `python3` support.

```
$ sudo pip3 install virtualenv # Any OS
```

Then create a Python virtual environment (abbreviated *virtualenv*). You could
call it anything, but we shall call it "ledger". This will create a directory
called "ledger" containing the virtualenv:

```
$ virtualenv ledger # Any OS
```

Then, you must enter the `virtualenv`. If you do not successfully enter the `virtualenv`,
future commands will fail. You can tell you have entered the virtualenv when your prompt is
prefixed with `(ledger)`.

```
$ source ledger/bin/activate
```

Your terminal session -- and only that terminal session -- will now be in the
virtual env. To have a new terminal session enter the virtualenv, run the above
`source` command only in the same directory in the new terminal session.

### ledgerblue: The Python Module for Ledger Nano S

We can now install `ledgerblue`, which is the Python module designed originally for
Ledger Blue, but also is needed for the Ledger Nano S.

Although we do not yet support Ledger Blue, you must still install the following python package.
Within the virtualenv environment -- making sure that `(ledger)` is showing up
before your prompt -- use pip to install the `ledgerblue`
[Python package](https://pypi.org/project/ledgerblue/).
This will install the Ledger Python packages into the virtualenv; they will be
available only in a shell where the virtualenv has been activated.

```
$ pip install ledgerblue
```

If you have to use `sudo` or `pip3` here, that is an indication that you have
not correctly set up `virtualenv`. It will still work in such a situation, but
please research other material on troubleshooting `virtualenv` setup.

### Load the app onto the Ledger device

Next you'll use the installation script to install the app on your Ledger Nano
S.

The Ledger hardware wallet must be in the following state:

  * Plugged into your computer
  * Unlocked (enter your PIN)
  * On the home screen (do not have any app open)
  * Not asleep (you should not see *vires in numeris* is scrolling across the
    screen)

If you are already in an app or the Ledger device is asleep, your installation process
will fail.

We recommend staying at your computer and keeping an eye on the Ledger device's screen
as you continue. You may want to read the rest of these instructions before you
begin installing, as you will need to confirm and verify a few things during the
process.

Still within the virtualenv, run the `./install.sh` command included in the `release.tar.gz`
that you downloaded.

This `./install.sh` script takes the path to an app directory. Two such directories
were included in the downloaded `release.tar.gz`.
Install both apps like this: `./install.sh wallet baking`.

The first thing that should come up in your terminal is a message that looks
like this:

```
Generated random root public key : <long string of digits and letters>
```

Look at your Ledger device's screen and verify that the digits of that key match the
digits you can see on your terminal. What you see on your Ledger hardware wallet's screen
should be just the beginning and ending few characters of the longer string that
printed in your terminal.

You will need to push confirmation buttons on your Ledger Nano S a few times
during the installation process and re-enter your PIN code near the end of the
process. You should finally see the Tezos logo appear on the screen.

If you see the "Generated random root public key" message and then something
that looks like this:

```
Traceback (most recent call last):
File "/usr/lib/python3.6/runpy.py", line 193, in _run_module_as_main
<...more file names...>
OSError: open failed
```

the most likely cause is that your `udev` rules are not set up correctly, or you
did not unplug your Ledger hardware wallet between setting up the rules and attempting to
install. Please confirm the correctness of your `udev` rules.

To load a new version of the Tezos application onto the Ledger Nano S in the future,
you can run the command again, and it will automatically remove any
previously-loaded version.

### Removing Your App

If you'd like to remove your app, you can do this. In the virtualenv
described in the last sections, run this command:

```
$ python -m ledgerblue.deleteApp --targetId 0x31100004 --appName 'Tezos Wallet'
```

Replace the `appName` parameter "Tezos" with whatever app name you used when you
loaded the app onto the device.

Then follow the prompts on the Ledger Nano S screen.

### Confirming the Installation Worked

You should now have two apps, `Tezos Baking` and `Tezos Wallet`. The `Tezos
Baking` app should display a `0` on the screen, which is the highest block
level baked so far (`0` in case of no blocks). The `Tezos Wallet` app will just
display `Tezos`.

## Registering the Ledger Nano S with the node

For the remainder of this document, we assume you have a Tezos node running and
`tezos-client` installed. Also, Docker has some issues working with the Ledger device,
so unless you're willing to troubleshoot them, we don't recommend it.

Currently there are two other ways to do this:

  1. If you have the Nix package manager, use the
     [Tezos baking platform](https://gitlab.com/obsidian.systems/tezos-baking-platform).
  2. Build tezos from the tezos repo with [these instructions](http://tezos.gitlab.io/mainnet/introduction/howtoget.html#build-from-sources).

Depending on how you build it, you might need to prefix `./` to your commands, and the names
of some of the binaries might be different.

### What is tezos-client

We can call the network at large "Tezos." Tezos consists of a bunch of nodes,
one of which is yours. Your node can be thought of as your gateway to the wider
network.

You can't do anything with the Ledger hardware wallet without using `tezos-client`. Tezos-client
is the program you use to access information about the network,
which you ultimately get through your node. See the
[command documentation](http://doc.tzalpha.net/api/cli-commands.html)
for the full array of features that tezos-client supports.

In summary:

* Tezos is the network
* We connect to the network through a node
* We access that node through tezos-client
* We store our client's keys on the Ledger device

Note that `tezos-client` will not only not support certain commands unless the node is installed,
but the error messages for those commands will not even indicate that those commands are possible.
If a command documented here gives an `Unrecognized command` error, make sure you have a node
running.

### Side note about key generation

Every Ledger hardware wallet generates public and private keys for `ed25519`, `secp256k1`, or
`P-256` encryption systems based on a seed (represented by and encoded in
the words associated with that Ledger device) and a BIP32 ("hierarchical deterministic
wallet") path.

The same seed and BIP32 path will always result in the same key for the same
systems. This means that, to keep your Bitcoin app from knowing your Tezos keys,
and vice versa, different BIP32 paths have to be used for the same Ledger device. This
also means that, in order to sync two Ledger devices, you can set them to the same
seed, represented as 24 or some other number of natural language words (English
by default).

All Tezos BIP32 paths begin with `44'/1729'` (the `'` indicates it is
"hardened").  Which Ledger Nano S is intended to be used, as well as choice of
encryption system, is indicated by a root key hash, the Tezos-specific base58
encoding of the hash of the public key at `44'/1729'` on that Ledger Nano S. Because
all Tezos paths start with this, in `tezos-client` commands it is implied.

### Importing the key from the Ledger Nano S

This section must be done regardless of whether you're going to be baking or
only using the Tezos Wallet application.

Please run, with a Tezos app open on your device (either Tezos Baking or Tezos Wallet will do):

```
$ tezos-client list connected ledgers
```

The output of this command includes three Tezos addresses derived from the secret
stored on the device, via different signing curves and BIP32 paths.

```
Found a Tezos Wallet 1.4.0 (commit 764625b1) application running on Ledger Nano S at [0001:002a:00].

To use keys at BIP32 path m/44'/1729'/0'/0' (default Tezos key path), use one of
 tezos-client import secret key ledger_jhartzell "ledger://major-squirrel-thick-hedgehog/ed25519/0'/0'"
 tezos-client import secret key ledger_jhartzell "ledger://major-squirrel-thick-hedgehog/secp256k1/0'/0'"
 tezos-client import secret key ledger_jhartzell "ledger://major-squirrel-thick-hedgehog/P-256/0'/0'"
```

These show you how to import keys with a specific signing curve (e.g. `ed25519`) and derivation path (e.g. `/0'/0'`). The
animal-based name (e.g. `major-squirrel-thick-hedgehog`) is a unique identifier for your
Ledger device enabling the client to distinguish different Ledger devices. This is combined with
a derivation path (e.g. `/0'/0'`) to indicate one of the possible keys on the Ledger Nano S. Your *root* key is the full identifier without the derivation path (e.g. `major-squirrel-thick-hedgehog/ed25519` by itself) but you should not use the root key directly\*.

\* *NOTE:* If you have used your root key in the past and need to import it, you can do so by simply running one of the commands but without the last derivation portion. From the example above, you would import your root key by running `tezos-client import secret key ledger_jhartzell "ledger://major-squirrel-thick-hedgehog/ed25519"`. You should avoid using your root key.

The Ledger Nano S does not currently support non-hardened path components. All
components of all paths must be hardened, which is indicated by following them
with a `'` character. This character may need to be escaped from the shell
through backslashes `\` or double-quotes `"`.

You'll need to choose one of the three commands starting with
`tezos-client import secret key ...` to run. `ed25519` is the standard recommended curve.

The BIP32 path is the part that in the example commands read `0'/0'`. You
can change it, but if you do (and even if you don't), be sure to write
down. You need the full address to use your tez. This means that if you
lose all your devices and need to set everything up again, you will need
three things:

  1. The mnemonic phrase -- this is the phrase from your Ledger device itself when you set it up, not the animal mnemonic you see on the command line. They are different.
  2. Which signing curve you chose
  3. The BIP32 path, if you used one

The `tezos-client import secret key` operation copies only the public key; it
says "import secret key" to indicate that the Ledger hardware wallet's secret key will be
considered available for signing from them on, but it does not leave the Ledger Nano S.

This sends a BIP32 path to the device. You then need to click a button on the
Ledger device, and the Ledger Nano S then sends the public key back to the computer.

After you perform this step, if you run the `list known addresses` command, you
should see the key you chose in the list:

```
$ tezos-client list known addresses
ledger_<...>_ed_0_0: tz1ccbGmKKwucwfCr846deZxGeDhiaTykGgK (ledger sk known)
```

We recommend reading as much as possible about BIP32 to ensure you fully understand
this.

## Using the Tezos Wallet application

This application and the Tezos Baking Application constitute complementary apps
for different use cases -- which could be on paired devices and therefore use
the same key, or which could also be used in different scenarios for different
accounts. Baking is rejected by this app.

The "provide address" command on the Tezos Wallet application shows the address
the first time the command is run for any given session. Subsequently, it
provides the address without prompting. To display addresses again, exit the
Wallet Application and restart it. This is again provided for testing/initial
set up purposes.

The sign command for the Wallet Application prompts every time for transactions
and other "unsafe" operations, with the generic prompt saying "Sign?" We hope to
eventually display more transaction details along with this. When block headers
and endorsements are sent to the Ledger Nano S, they are rejected silently as if the
user rejected them.

### Faucet (zeronet only)

On zeronet, you will need to use the [Tezos Faucet](https://faucet.tzalpha.net/)
to obtain some tez. Tell them you're not a robot, then click "Get alphanet tz."
It works on zeronet (even though the URL says alphanet).

Run the following command, where `<your-name>` is some alias you want to use for
this wallet, and `tz1<...>.json` is the name of the file you just downloaded
from the faucet.

```
$ tezos-client activate account <your-name> with ~/downloads/tz1<...>.json
Node is bootstrapped, ready for injecting operations.
Operation successfully injected in the node.
Operation hash is 'onxJStKxK1oMPgGskkzc2gDBDyKeQ7CbBYTrcK4TMMySvKZq6vF'.
Waiting for the operation to be included...
Operation found in block: BMRjW94ge499sCPAMUTvrp3ku2UjWy9kB2LsjtuJhL1bkcQ85Ny (pass: 2, offset: 0)
This sequence of operations was run:
  Genesis account activation:
    Account: tz1Vntj2aVpqcQEeHq2CEmNrSGw8finvbFcX
    Balance updates:
      tz1Vntj2aVpqcQEeHq2CEmNrSGw8finvbFcX ... +ꜩ66835.212314

Account <your-name> (tz1Vntj2aVpqcQEeHq2CEmNrSGw8finvbFcX) activated with ꜩ66835.212314.
```

You can then check your account balance like this:

```
$ tezos-client get balance for <your-name>
66835.212314 ꜩ
```

### Transfer

Now transfer the balance to the account whose key resides on your Ledger Nano S:

```
$ tezos-client transfer 66000 from chris-martin2 to ledger_<...>_ed_0_0
```

### Further transaction details

In general, to send tez, you'll need to:

  * Have a node running
  * Open the Tezos Wallet app on your hardware wallet
  * Know the alias of your account or its public key hash
  * Know the public key hash of the account you are sending tez to

The command you run has the form:

```
tezos-client transfer QTY from SRC to DST
```

  * `QTY` is the amount of tez. It's best to not include commas and to include 6
    decimal points (ie. 1000000.000000). If you'd prefer to include commas, you can:
    `1,000,000.000,000`.
  * `SRC` is the source, or where the money is coming from. This should be your
    alias or public key has.
  * `DST` is the destination, or where the money is going. You should use the
    public key hash, as your computer likely doesn't know any aliases for that
    account.

Some options which you can consider:

  * `--fee <amount>` - The fee defaults to 0.05 tez. If you'd like to select
    another amount, either because you think that's too high or the network is
    crowded and a higher fee is needed to ensure it goes through, you can
    include this with the amount of fee you want to pay (ie. `--fee 0.05` for
    the default).
  * `-D` or `--dry-run` - Use this if you just want to display what would happen
    and not actually do the transaction.
  * `-G` or `--gas-limit` - This sets the gas limit of the transaction instead.

There are other options which you can read up about more in the docs, but these
are the main ones you'd potentially want to use when just sending tez to
someone.

### Delegation

If you want to delegate tez controlled by a Ledger Nano S account to another account to
bake, that requires the Wallet App. This is distinct from registering the Ledger Nano S
account itself to bake, which is also called "delegation," and which is covered
in the section on the baking app below.

To delegate tez controlled by a Ledger device to someone else,
you must first originate an account. Please read more
about this in the Tezos document, [How to bake on the
alphanet](http://doc.tzalpha.net/introduction/alphanet.html#how-to-bake-on-the-alphanet), to
understand why this is necessary and the semantics of delegation.

To originate an account, the command is:
```
$ tezos-client originate account <NEW> for <MGR> transferring <QTY> from <SRC> --delegatable
```

  * `NEW` is the alias you will now give to the originated account. Only originted accounts can
     be delegated, and even then only if originated with this `--delegatable` flag.
  * `MGR` is the name of the key you will use to manage the account. If you want to manage it
    with a Ledger device, it should be an existing imported key from the Ledger hardware wallet.
  * `QTY` is the initial amount of tez to give to the originated account.
  * `SRC` is the account where you'll be getting the tez from.

Subsequently, every transaction made with `<NEW>` will require the Ledger harware wallet mentioned in `<MGR>`
to sign it. This is done with the wallet application, and includes setting a delegate with:

```
$ tezos-client set delegate for <NEW> to <DELEGATE>
```

Originated accounts have names beginning with `KT1` rather than `tz1`, `tz2` or `tz3`.

### Proposals and Voting

To submit (or upvote) a proposal during the Proposal Period, open the Wallet app on your ledger and run

```
$ tezos-client submit proposals for <ACCOUNT> <PROTOCOL-HASH>
```

The Wallet app will then ask you to confirm the various details of the proposal submission.

**Note:** While `tezos-client` will let you submit multiple proposals at once with this command, submitting more than one will cause the Wallet app to show "Sign Unverified?" instead of showing each field of each proposal for your confirmation. Signing an operation that you can't confirm is not safe and it is highly recommended that you simply submit each proposal one at a time so you can properly confirm the fields on the ledger device.

Voting for a proposal during the Exploration or Promotion Vote Period also requires that you have the Wallet app open. You can then run

```
$ tezos-client submit ballot for <ACCOUNT> <PROTOCOL-HASH> <yea|nay|pass>
```

The Wallet app will ask you to confirm the details of your vote.

Keep in mind that only registered delegate accounts can submit proposals and vote. Each account can submit up to 20 proposals per proposal period and vote only once per voting period. For a more detailed post on participating during each phase of the amendment process, see this [Medium post](https://medium.com/@obsidian.systems/voting-on-tezos-with-your-ledger-nano-s-8d75f8c1f076). For a full description of how voting works, refer to the [Tezos documentation](https://gitlab.com/tezos/tezos/blob/master/docs/whitedoc/voting.rst).

## Using the Tezos Baking Application

The Tezos Baking Application supports 3 operations:

  1. Authorize/get public key
  2. Reset high watermark
  3. Sign

It will only sign block headers and endorsements, as the purpose of the baking
app is that it cannot be co-opted to perform other types of operations (like
transferring XTZ). If a Ledger Nano S is running with the Tezos Baking Application, it
is the expectation of its owner that no transactions will need to be signed with
it. To sign transactions with that Ledger Nano S, you will need to switch it to using
the Tezos Wallet application, or have the Tezos Wallet application installed on
a paired device. Therefore, if you have a larger stake and bake frequently, we
recommend the paired device approach. If, however, you bake infrequently and can
afford to have your baker offline temporarily, then switching to the Tezos
Wallet application on the same Ledger Nano S should suffice.


### Start the baking daemon

```
$ tezos-baker-003-PsddFKi3 run with local node ~/.tezos-node ledger_<...>_ed_0_0
```

This won't actually be able bake successfully yet until you run the rest of
these setup steps. This will run indefinitely, so you might want to do it in
a dedicated terminal or in a `tmux` or `screen` session.

You will also want to start the endorser and accuser daemons:

```
$ tezos-endorser-003-PsddFKi3 run ledger_<...>_ed_0_0
$ tezos-accuser-003-PsddFKi3 run
```

Again, each of these will run indefinitely, and each should be in its own terminal
`tmux`, or `screen` window.

*Note*: The binaries shown above all correspond to current Tezos mainnet protocol. When the Tezos protocol upgrades, the binaries shown below will update to, for instance, `tezos-baker-004-********`.

### Authorize public key

You need to run a specific command to authorize a key for baking. Once a key is
authorized for baking, the user will not have to approve this command again. If
a key is not authorized for baking, signing endorsements and block headers with
that key will be rejected. This authorization data is persisted across runs of
the application.  Only one key can be authorized for baking per Ledger hardware wallet at a
time.

In order to authorize a public key for baking, you can do either of the
following:

  * Use the APDU for authorizing a public key.

    The command for this is as follows:

    ```
    $ tezos-client authorize ledger to bake for <SIGNATURE>
    ```

    This only authorizes the key for baking on the Ledger Nano S, but does
    not inform the blockchain of your intention to bake. This might
    be necessary if you re-install the app, or if you have a different
    paired Ledger device that you are using to bake for the first time.

  * Have the baking app sign a self-delegation. This is explained in the next section.

### Registering as a Delegate

In order to bake from the Ledger Nano S account you need to register the key as a
delegate. This is formally done by delegating the account to itself. As a
non-originated account, an account directly stored on the Ledger device can only
delegate to itself.

Open the Tezos Baking Application on the device, and then run this:

```
$ tezos-client register key ledger_<...>_ed_0_0 as delegate
```

This command is intended to inform the blockchain itself of your intention to
bake with this key. It can be signed with either the Wallet App or the Baking
App, but if you sign it with the Baking App, it also implies to the Ledger Nano S
that you want to authorize that key for baking on the device as well.

Authorizing a key for baking on a specific Ledger Nano S and authorizing it for
baking in general on the blockchain are two distinct authorizations. This
command will do both of them if signed with the appropriate baking app,
whereas the `authorize ledger` command in the previous section will
only do it for the in question, which is appropriate if it is already
authorized to bake on the blockchain.

The `register key` command is equivalent to:

```
$ tezos-client set delegate for ledger_<...>_ed_0_0 to ledger_<...>_ed_0_0
```

The Baking App only signs self-delegations; the Wallet App is needed to sign
delegations of originated accounts controlled by a hardware wallet. The Baking App
also only signs delegations with fees less than 0.05 ꜩ; to sign those with
more, you must use the Wallet App to authorize baking for the blockchain,
and the command in the previous section to authorize baking for the Ledger Nano S,
which is always available as an alternative to signing this with the Baking App.

### Sign

The sign operation is for signing block headers and endorsements.

Block headers must have monotonically strictly increasing levels; that is, each
block must have a higher level than all previous blocks signed with the Ledger device.
This is intended to prevent double baking at the device level, as a security
measure against potential vulnerabilities where the computer might be tricked
into double baking. This feature will hopefully be a redundant precaution, but
it's implemented at the device level because the point of the Ledger hardware wallet is to not
trust the computer. The current High Watermark (HWM) -- the highest level to
have been baked so far -- is displayed on the device's screen, and is also
persisted between runs of the device.

The sign operation will be sent to the hardware wallet by the baking daemon when
configured to bake with a Ledger Nano S key. The Ledger device uses the first byte of the
information to be signed -- the magic number -- to tell whether it is a block
header (which is verified with the High Watermark), an endorsement (which is
not), or some other operation (which it will reject, unless it is a
self-delegation).

With the exception of self-delegations, as long as the key is configured and the
high watermark constraint is followed, there is no user prompting required for
signing. The baking app will only ever sign without prompting or reject an
attempt at signing; this operation is designed to be used unsupervised.

As mentioned, the only exception to this is self-delegation. This will prompt
and display the public key hash of the account. This will authorize the key for
baking on the hardware wallet. This is independent of what the signed self-delegation
will then do on the blockchain. If you are baking on a new device, or have
reinstalled the app, you might have to sign a self-delegation to authorize the
key on the Ledger Nano S, even if you are already registered for baking on the
block-chain. This will also have to be done if you have previously signed this
with the wallet app.

### Reset

There is some possibility that the HWM will need to be reset. This could be due
to the computer somehow being tricked into sending a block with a high level to
the Ledger Nano S (either due to an attack or a bug on the computer side), or because
the owner of the device wants to move from the real Tezos network to a test
network, or to a different test network. This can be accomplished with the reset
command.

This is intended as a testing/debug tool and as a possible recovery from
attempted attacks or bugs, not as a day to day command. It requires an explicit
confirmation from the user, and there is no specific utility to send this
command. To send it manually, you can use the following command line:

`tezos-client set ledger high watermark for "ledger://<tz...>/" to <HWM>`

`<HWM>` indicates the new high watermark to reset to. If you are joining a new
test network, `0` is a fitting number, as then all blocks will be allowed again.
You can also set it to the most recently baked level on that test net.

If you are not physically present at your computer, but are curious what the
Ledger device's high watermark is, you can run:

`tezos-client get ledger high watermark for "ledger://<tz...>/"`

## Upgrading

When you want to upgrade to a new version, whether you built it yourself from source
or whether it's a new release of the `app.hex` files, use the same commands as you did
to originally install it. As the keys are generated from the device's seeds and the
derivation paths, you will have the same keys with every version of this Ledger Nano S app,
so there is no need to re-import the keys with `tezos-client`.

### Special Upgrading Considerations for Bakers

If you've already been baking on an old version of the Baking App, the new version will
not remember which key you are baking with nor the High Watermark. You will have to re-run
this command to remind the hardware wallet what key you intend to authorize for baking:

```
$ tezos-client authorize ledger to bake for <SIGNATURE>
```

You can also set the High Watermark to the level of the most recently baked block with:

```
tezos-client set ledger high watermark for "ledger://<tz...>/" to <HWM>
```

This will require the correct URL for the Ledger Nano S acquired from:

```
tezos-client list connected ledgers
```

## Troubleshooting

### Display Debug Logs

If you are worried about bugs, you should configure your system to display debug logs. Add the
following line to `~/.bashrc` and to `~/.bash_profile`, or set the equivalent environnment
variable in whatever system you use to launch your daemons:

```
export TEZOS_LOG="client.signer.ledger -> debug"
```

If you have a bug report, it is far more likely we'll be able to fix it if you include the
entire output of the transaction, including debug messages enabled by that command above.
Please copy and paste the entire run of the command (for `tezos-client`) or everything
involving the failed block level and the previous one (for baking); if you need to anonymize
the PKH then please do so by using `XXX` or similar rather than by removing those entire lines.
We need as much context as possible to help troubleshoot.

`script` is also a useful command for logging all the output of a long-running process.
If you run `script <file-name>` it opens a new shell where everything output and typed
is also output to that file, giving you a transcript of your terminal session.

### Importing a Fundraiser Account to a Ledger Device

You currently cannot directly import a fundraiser account to the Ledger device. Instead, you'll first need to import your fundraiser account to a non-hardware wallet address from which you can send the funds to an address on the ledger. You can do so with wallet providers such as [Galleon](https://galleon-wallet.tech/) or [TezBox](https://tezbox.com/).

### Two Ledger Devices at the Same Time

Two Ledger devices with the same seed should not ever be plugged in at the same time. This confuses
`tezos-client` and other client programs. Instead, you should plug only one of a set of paired
ledgers at a time. Two Ledger devices of different seeds are fine and are fully supported,
and the computer will automatically determine which one to send information to.

If you have one running the baking app, it is bad for security to also have the wallet app
plugged in simultaneously. Plug the wallet app in as-needed, removing the baking app, at a time
when you are not going to be needed for endorsement or baking. Alternatively, use a different
computer for wallet transactions.

### unexpected seq num

```
$ client/bin/tezos-client list connected ledgers
Fatal error:                                                                                                                                        Header.check: unexpected seq num
```

This means you do not have the Tezos app open on your device.

### No device found

```
$ tezos-client list connected ledgers
No device found.
Make sure a Ledger Nano S is connected and in the Tezos Wallet app.
```

In addition to the possibilities listed in the error message, this could also
mean that your udev rules are not set up correctly.

### Unrecognized command

If you see an `Unrecognized command` error, it might be because there is no node for `tezos-client`
to connect to. Please ensure that you are running a node. `ps aux | grep tezos-node` should display
the process information for the current node. If it displays nothing, or just displays a `grep`
command, then there is no node running on your machine.

### Ledger Nano S App Crashes

If the Ledger Nano S app crashes when you load it, there are two primary causes:

  * Quitting the `tezos-client` process before the device responds. Even if you meant to cancel
    the operation in question, cancel it from the device before pressing Ctrl-C, otherwise you
    might have to restart the Ledger Nano S.
  * Out of date firmware: If the Ledger Nano S app doesn't work at all, make sure you are running firmware
    version 1.5.5.

### Contact Us
 You can email us at tezos@obsidian.systems and request to join our Slack.
We have several channels about baking and one specifically for our Ledger Nano S apps.
You can ask questions and get answers from Obsidian staff or from the community.
