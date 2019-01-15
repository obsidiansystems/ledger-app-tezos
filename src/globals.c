#include "globals.h"

#include <string.h>

globals_t global;

// These are strange variables that the SDK uses directly.
ux_state_t ux;
unsigned char G_io_seproxyhal_spi_buffer[IO_SEPROXYHAL_BUFFER_SIZE_B];

void init_globals(void) {
  memset(&global, 0, sizeof(global));

  // DO NOT INITIALIZE THESE. They are, apparently, initialized by the SDK.
  //memset(&ux, 0, sizeof(ux));
  //memset(G_io_seproxyhal_spi_buffer, 0, sizeof(G_io_seproxyhal_spi_buffer));
}
