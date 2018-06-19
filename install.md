# How to load apps onto your Ledger device

These are the instructions you need if you have an `app.hex` file containing a
Tezos Ledger application and you want to load it onto a Ledger device.

Ledger's primary interface for loading an app onto a Ledger device is a Chrome
app called [Ledger Manager][ledger-manager]. However, it only supports a limited
set of officially-supported apps like Bitcoin and Ethereum, and Tezos is not yet
among these. So you will need to use a command-line tool called the [BOLOS
Python Loader][bolos-python-loader]. This document describes how to install this
Python library and its dependencies, and how to use it to install a Tezos app
onto a Ledger device.

## Permissions

On Linux, you need to set some udev rules to set the permissions so your user
can access the Ledger device.

* For most distros, follow the [Ledger support instructions][ledger-udev].
* For NixOS, see the Nix-specific instructions further below.

Remember that if you change udev rules while your Ledger is still plugged in,
you need to unplug it and then plug it back in so the rules can take effect.

## `app.hex`

Place the built Ledger app at `bin/app.hex` in this repository.

## libusb

Install `libusb` and `libudev`. On Ubuntu, you can do this with:

```bash
sudo apt-get install libusb-1.0-0-dev libudev-dev
```

## pip and virtualenv

Install `pip`. On Ubuntu, you can do this with:

```bash
sudo apt-get install python3-pip
```

Now, on any operating system, install `virtualenv` using `pip3`. It is important to use
`pip3` and not `pip` for this, as this module requires `python3` support.

```bash
sudo pip3 install virtualenv
```

Then create a Python virtual environment (abbreviated *virtualenv*). You could
call it anything, but here we call it "ledger". This will create a directory
called "ledger" containing the virtualenv.

```bash
virtualenv ledger
```

Enter a new shell and source the virtualenv's `activate` script to set up the
virtualenv in it.

```bash
bash
source ledger/bin/activate
```

## ledgerblue

Within the virtualenv environment, use pip to install the [`ledgerblue` Python
package][pypi-ledgerblue]. This will install the Ledger Python packages into the
virtualenv; they will be available only in a shell where the virtualenv has been
activated.

```bash
pip install ledgerblue
```

## Load the app onto the Ledger device

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

Still within the virtualenv, run the `./install.sh` command. This script
takes two parameters, the first of which is the *name* of the application
you are installing, and the second is the path to the `app.hex` file:

* If you are installing the baking app, we recommend using the name "Tezos
  Baking".

  ```bash
  ./install.sh "Tezos Baking" bin/app.hex
  ```

* If you are installing the transaction app, we recommend using the name "Tezos
  Wallet".

  ```bash
  ./install.sh "Tezos Wallet" bin/app.hex
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
install. Refer to the Ledger documentation linked above (or check your NixOS
configuration if you are a NixOS user and ensure you did a proper *rebuild* to
ensure your rules are correct). Once you have confirmed they are correct, unplug
your Ledger and plug it back in before running the installation script again.

To load a new version of the Tezos application onto the Ledger in the future,
you can run the command again, and it will automatically remove any
previously-loaded version.

## Removing the app from a Ledger device

If you wish to fully remove Tezos from your Ledger device, you may do so by
running this command (in the virtualenv):

```
python \
  -m ledgerblue.deleteApp \
  --targetId 0x31100003 \
  --appName "Tezos"
```

Replace the `appName` parameter "Tezos" with whatever app name you used when you
loaded the app onto the device.

Then follow the prompts on the Ledger screen.

## Nix

If you have the [Nix package manager][nix] installed, then you can skip all of
the dependency setup instructions above (libusb, virtualenv, and ledgerblue) and
install by using the [Tezos Baking Platform](https://gitlab.com/obsidian.systems/tezos-baking-platform).
In the `ledger` directory of that repo, on the `ledger-baking` branch, run:

```bash
./setup_ledger.sh
```

This will install both apps.

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

Once you have added this, run `sudo nixos-rebuild switch` to activate the configuration.

  [ledger-udev]: https://support.ledgerwallet.com/hc/en-us/articles/115005165269
  [ledger-manager]: https://www.ledgerwallet.com/apps/manager
  [bolos-python-loader]: https://ledger.readthedocs.io/projects/blue-loader-python/en/0.1.16/index.html
  [pypi-ledgerblue]: https://pypi.org/project/ledgerblue/
  [nix]: https://nixos.org/nix/
