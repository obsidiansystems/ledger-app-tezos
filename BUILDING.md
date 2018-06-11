# Ledger-app-tezos -- Build Instructions

## Building

If you already have an `app.hex` file or files that you want to
use, e.g. from a release, then you do not need these instructions.
Please go to the `README.md` file's for installation instructions.

These instructions are designed for Linux.

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
environment variable undefined.

Once we have done more testing, we will also release the `app.hex` files directly.

To install on the ledger, please proceed to the Installation instructions
in the main `README.md` file.
