#include "globals.h"

#include <string.h>

// WARNING: ***************************************************
// Non-const globals MUST NOT HAVE AN INITIALIZER.
//
// Providing an initializer will cause the application to crash
// if you write to it.
// ************************************************************


globals_t global;

// These are strange variables that the SDK relies on us to define but uses directly itself.
ux_state_t ux;
unsigned char G_io_seproxyhal_spi_buffer[IO_SEPROXYHAL_BUFFER_SIZE_B];

// DO NOT TRY TO INIT THIS. This can only be written via an system call.
// The "N_" is *significant*. It tells the linker to put this in NVRAM.
WIDE nvram_data N_data_real; // TODO: What does WIDE actually mean?

void init_globals(void) {
  memset(&global, 0, sizeof(global));
  memset(&ux, 0, sizeof(ux));
  memset(G_io_seproxyhal_spi_buffer, 0, sizeof(G_io_seproxyhal_spi_buffer));
}
