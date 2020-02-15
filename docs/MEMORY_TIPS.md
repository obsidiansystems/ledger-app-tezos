Debugging Memory Problems with ledger-app-tezos
==============================================

A typical reason for most problems with the app is lack of
memory. Indeed, the stack and the global variables share a small
read-write memory space. The more variables, the less space for the
stack. When the stack overflows, variables are corrupted, causing a
failure of the application.

The current `Makefile` uses:

```
DEFINES   += HAVE_BOLOS_APP_STACK_CANARY
```

With this line, BOLOS introduces a special variable, a canary, at the
end of the stack, and it checks whether it is not modified after every
message. If corruption is detected, the application exits (instead of
freezing later).

If you experiment this, you should create a bug report. You can also
try the following tips to find the problem.

Adding Debug Support
--------------------

First, you will need to install a new firmware on your Ledger. Follow
the documentation for that, to get the `PRINTF` feature working (you
will need [`usbtool`](https://ledger.readthedocs.io/en/latest/userspace/debugging.html#console-printing) on your computer too).

Then, modify the `Makefile` to set the `DEBUG` variable to `1`.  `make
clean && make`, and your application can use the `PRINTF` macro to
send information to the computer while running.

You can use `make watch` to start watching the USB log to see the
messages sent by `PRINTF`.

Printing Stack Size
-------------------

Go to your copy of `nanos-secure-sdk`, and use the patch
`nanos-secure-sdk.patch` in the `docs/` directory of
`ledger-app-tezos`.

```
patch -p1 < ../docs/nanos-secure-sdk.patch
```

Now, in the `Makefile`, set the variable `DEBUGMEM` to `1`. `make
clean && make` and your application can use the
`PRINTF_STACK_SIZE("fun_name")` macro.

This macro prints (to the computer USB log) the remaining stack size
at the beginning of a function, once all the space for the variables
in the function has been reserved. Some functions can make a big use
of stack, for example Blake2b functions are expensive (around 400
bytes needed for `b2b_compress`).

Note that `PRINTF_STACK_SIZE` itself has a cost on stack, so, if you
reduce the stack usage, you may still need to remove
`PRINTF_STACK_SIZE` for the application to perform correctly.

Fixing Stack Problems
---------------------

Once you have discovered which stack overflows, you will need to find
a way to reduce the stack usage of some of the functions.

For that, here are a few tips and info:

1. The compiler performs aggressive inlining of functions. Sometimes,
the frame reserved for a function is bigger than it should be, because
it contains the frames of some inline functions. Inlining may also on
the contrary reduce stack frames. So, you will need to investigate by
using the attributes `inline` (to force inlining) and
`__attribute__ ((noinline))` (to prevent inlining).

2. To know which functions are inlined, and the size of the stack
frame of a function, you will need to look at the generated assembly
code. For that, use `make -n` to see how the compiler is called, and
add the `-S` flag to print the assembly code (you will also need to
change the target of `-o`). For example:

```
ledger-env/clang-arm-fropi/bin/clang -c -gdwarf-2  -gstrict-dwarf -O3 -Os  -I/usr/include -fomit-frame-pointer -mcpu=cortex-m0 -mthumb  -fno-common -mtune=cortex-m0 -mlittle-endian  -std=gnu99 -Werror=int-to-pointer-cast -Wall -Wextra  -fdata-sections -ffunction-sections -funsigned-char -fshort-enums  -mno-unaligned-access  -Wno-unused-parameter -Wno-duplicate-decl-specifier -fropi --target=armv6m-none-eabi -fno-jump-tables  -O3 -Os -Wall -Wextra  -DST31 -Dgcc -D__IO=volatile -DOS_IO_SEPROXYHAL -DHAVE_BAGL -DHAVE_SPRINTF -DHAVE_IO_USB -DHAVE_L4_USBLIB -DIO_USB_MAX_ENDPOINTS=6 -DIO_HID_EP_LENGTH=64 -DHAVE_USB_APDU -DVERSION=\"0.3.1-20190824\" -DCOMMIT=\"7c3880c3\" -DAPPVERSION_M=0 -DAPPVERSION_N=3 -DAPPVERSION_P=1 -DAPPVERSION_D=\"20190824\" -DHAVE_U2F -DHAVE_IO_U2F -DU2F_PROXY_MAGIC=\"XTZ\" -DUSB_SEGMENT_SIZE=64 -DBLE_SEGMENT_SIZE=32 -DHAVE_BOLOS_APP_STACK_CANARY -DIO_SEPROXYHAL_BUFFER_SIZE_B=128 -DHAVE_PATCHED_BOLOS -DHAVE_PRINTF -DPRINTF=screen_printf -I$BOLOS_SDK/lib_stusb/ -I$BOLOS_SDK/lib_stusb/STM32_USB_Device_Library/Class/CCID/inc/ -I$BOLOS_SDK/lib_stusb/STM32_USB_Device_Library/Class/CCID/inc/ -I$BOLOS_SDK/lib_stusb/STM32_USB_Device_Library/Class/CCID/inc/ -I$BOLOS_SDK/lib_stusb/STM32_USB_Device_Library/Class/CCID/inc/ -I$BOLOS_SDK/lib_stusb/STM32_USB_Device_Library/Class/HID/Inc/ -I$BOLOS_SDK/lib_stusb/STM32_USB_Device_Library/Core/Inc/ -I$BOLOS_SDK/lib_stusb/STM32_USB_Device_Library/Core/Inc/ -I$BOLOS_SDK/lib_stusb/STM32_USB_Device_Library/Core/Inc/ -I$BOLOS_SDK/lib_stusb/STM32_USB_Device_Library/Core/Inc/ -I$BOLOS_SDK/lib_stusb/STM32_USB_Device_Library/Core/Inc/ -I$BOLOS_SDK/lib_stusb_impl/ -I$BOLOS_SDK/lib_stusb_impl/ -I$BOLOS_SDK/lib_stusb_impl/ -I$BOLOS_SDK/lib_u2f/include/ -I$BOLOS_SDK/lib_u2f/include/ -I$BOLOS_SDK/lib_u2f/include/ -I$BOLOS_SDK/lib_u2f/include/ -I$BOLOS_SDK/lib_u2f/include/ -Iinclude -I$BOLOS_SDK/include -I$BOLOS_SDK/include/arm -Isrc/ -S -o code.asm src/parse_operations.c
```

will print in file `code.asm` the generate assembly for
`parse_operations.c` (note that we modified `-DCOMMIT` and `-DUNUSED`
to prevent some problems with characters `*` and `()`).

To understand what is happending, you need to know:

* every function has a line with a tag `%function`. If it is does not
  have such a line, then the function has been completely inlined in
  other functions

* there are annotations `.cfi_def_cfa_offset` that give the size of
  the frame for a function at any given point.

3. Finally, since the stack shares memory with global variables,
another solution to solve stack issues is to reduce the size of global
variables.
