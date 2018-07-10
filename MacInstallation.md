# Installing Tezos Applications on the Ledger Nano S

## Overview
These installation instructions have been adapted from our [README](https://github.com/obsidiansystems/ledger-app-tezos/blob/master/README.md) to be mac-specific. If you have questions regarding this installation, we recommend you refer to those instructions.

**Note** - this guide makes 2 assumptions:
1. You have [XCode](https://developer.apple.com/xcode/) and [Homebrew](https://brew.sh/) installed. Both are available for free to download via those links.
2. You have updated your Ledger firmware to 1.4.2. You can do so through [Ledger Manager](https://www.ledgerwallet.com/apps/manager) or [Ledger Live](https://www.ledgerwallet.com/live).

### Installing Python3

First, check if you have Python3 installed:

`python3 --version`

If this returns a version, such as `Python 3.6.5`, you can skip to the next section. If it returns nothing or you get an error, such as ‘command not found’, you’ll need to install it with homebrew:

`brew install python`

This step may take some time. When complete, you can confirm with `python3 --version`. It should now return a version.

### Installing Virtualenv

Check if you have virtualenv installed:

`virtualenv --version`

If this returns a version, such as `16.0.0`, you can skip to the next section. If it returns nothing or you get an error, such as ‘command not found’, you’ll need to install it:

`pip3 install virtualenv`

After that successfully installs, running `virtualenv --version` should now return it’s version number.

### Clone This Repo

Clone the Obsidian Systems Ledger App repo:

`git clone https://github.com/obsidiansystems/ledger-app-tezos.git`

This gives you all the tools to install the applications, but not the app files themselves. We’ll get those later. Enter the folder you just downloaded.

`cd ledger-app-tezos`

### Run virtualenv

From within `/ledger-app-tezos`, run the following two commands:

`virtualenv ledger -p python3`

`source ledger/bin/activate`

Your terminal session (and only that terminal session) will now be in the `virtualenv`. To have a new terminal session enter the `virtualenv`, run the above source command only in the same directory in the new terminal session.
In the shell that starts with (ledger), run (don’t use `sudo` or `pip3`!):

`pip install ledgerblue`

If you have to use `sudo` or `pip3` here, that is an indication that you have not correctly set up `virtualenv`. It will still work in such a situation, but please research other material on troubleshooting `virtualenv` setup.

### Download the Application(s)

Next you'll use the installation script to install the app on your Ledger Nano S. Let’s start by downloading the application(s) from here - `https://github.com/obsidiansystems/ledger-app-tezos/releases/`. Download the most recent version (currently 1.1 at the time of this writing). To download, use the `release.tar.gz` link - that’s where you’ll find the application hex files. Once you’ve downloaded them, move them into the `/ledger-app-tezos` folder.

### Install the Application(s)

We’re now ready to do the installation! The Ledger must be in the following state:
- Plugged into your computer
- Unlocked (enter your PIN)
- On the home screen (do not have any app open)
- Not asleep (you should not see vires in numeris is scrolling across the screen)

In `virtualenv`, in the `/ledger-app-tezos` folder, run the following:
- To install the Baking app - `./install.sh "Tezos Baking" baking.hex`
- To install the Wallet App - `./install.sh "Tezos Wallet" wallet.hex`

Pay attention to your Ledger during this process, as you’ll be prompted to confirm the installation.

You should now see the applications on your Ledger!

# Upgrading Applications

Periodically, we’ll update these applications. To do the upgrade yourself, run the same commands you used for the installation! Just make sure to update the hex files you are upgrading to the new version by moving the new `baking.hex` or `wallet.hex` file to `/ledger-app-tezos`. The installation script will automatically remove the old version of the app.

# Deleting Applications

In that same `virtualenv` we created to install the applications, run the following command to delete them:

`python -m ledgerblue.deleteApp --targetId 0x31100003 --appName "Tezos Baking"`

**Note**: The part in quotes needs to be the name of the application you are trying to delete. So if you are trying to delete the Wallet App, you should change it to say “Tezos Wallet”

# Troubleshooting

### Ledger Prompt: “Allow unknown manager?”

This prompt indicates that the application interacting with the Ledger is not the Ledger Manager or Ledger Live. It is expected, and not a cause for concern.

### Ledger Prompt: “Open non-genuine app?”

If the ledger says “Open non-genuine app?” then freezes, this is a sign that the install was unsuccessful. You probably need to update your Ledger’s firmware to 1.4.2, then restart the process.

### “Broken certificate chain - loading from user key”

If you are running the installation script and you already have a version of the application on your Ledger, you will see this message. This is normal, and not a cause for concern. 

### Removing a virtualenv

Each time you create a virtualenv, it makes a folder with that instance in it with the name you chose to call it. In these docs, we called it ledger. If you try and run virtualenv ledger and you’ve already made a ledger virtualenv, you’ll need to delete it first:

`/ledger-app-tezos$ rm -rf ledger`

Another alternative is to create future virtualenvs with a different name, such as ledger1.
