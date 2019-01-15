#include "globals.h"

#include <string.h>

globals_t global;

void init_globals(void) {
  memset(&global, 0, sizeof(global));
}
