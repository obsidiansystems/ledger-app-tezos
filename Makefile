#*******************************************************************************
#   Ledger Nano S
#   (c) 2016 Ledger
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#*******************************************************************************

ifeq ($(BOLOS_SDK),)
$(error Environment variable BOLOS_SDK is not set)
endif
include $(BOLOS_SDK)/Makefile.defines

ifeq ($(BAKING_APP),Y)
APPNAME = "Tezos Baking"
else
APPNAME = "Tezos Wallet"
endif
APP_LOAD_PARAMS=--appFlags 0 --curve ed25519 --curve secp256k1 --curve prime256r1 --path "44'/1729'" $(COMMON_LOAD_PARAMS)
VERSION_TAG=$(shell git tag | grep '^v' | sort | tail -n1)
APPVERSION_M=1
APPVERSION_N=4
APPVERSION_P=1
APPVERSION=$(APPVERSION_M).$(APPVERSION_N).$(APPVERSION_P)

ifneq (v$(APPVERSION), $(VERSION_TAG))
    $(warning "Version-Tag Mismatch: v$(APPVERSION) version and $(VERSION_TAG) tag")
endif

COMMIT := $(shell git describe --abbrev=8 --always)

ICONNAME=icon.gif
################
# Default rule #
################
all: default

############
# Platform #
############

DEFINES   += OS_IO_SEPROXYHAL IO_SEPROXYHAL_BUFFER_SIZE_B=128
DEFINES   += HAVE_BAGL HAVE_PRINTF
DEFINES   += HAVE_IO_USB HAVE_L4_USBLIB IO_USB_MAX_ENDPOINTS=6 IO_HID_EP_LENGTH=64 HAVE_USB_APDU
DEFINES   += VERSION=\"$(APPVERSION)\" APPVERSION_M=$(APPVERSION_M)
DEFINES   += COMMIT=\"$(COMMIT)\" APPVERSION_N=$(APPVERSION_N) APPVERSION_P=$(APPVERSION_P)



##############
#  Compiler  #
##############
GCCPATH   := $(BOLOS_ENV)/gcc-arm-none-eabi-5_3-2016q1/bin/
CLANGPATH := $(BOLOS_ENV)/clang-arm-fropi/bin/
CC       := $(CLANGPATH)clang

ifeq ($(BAKING_APP),)
CFLAGS   += -O3 -Os -Wall -Wextra
endif
ifeq ($(BAKING_APP),Y)
CFLAGS   += -DBAKING_APP -O3 -Os -Wall -Wextra
endif

AS     := $(GCCPATH)arm-none-eabi-gcc

LD       := $(GCCPATH)arm-none-eabi-gcc
LDFLAGS  += -O3 -Os
LDLIBS   += -lm -lgcc -lc

# import rules to compile glyphs(/pone)
include $(BOLOS_SDK)/Makefile.glyphs

### computed variables
APP_SOURCE_PATH  += src
SDK_SOURCE_PATH  += lib_stusb lib_stusb_impl

load: all
	python -m ledgerblue.loadApp $(APP_LOAD_PARAMS)

delete:
	python -m ledgerblue.deleteApp $(COMMON_DELETE_PARAMS)

# import generic rules from the sdk
include $(BOLOS_SDK)/Makefile.rules

#add dependency on custom makefile filename
dep/%.d: %.c Makefile
