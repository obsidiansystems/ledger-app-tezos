#include "os.h"
#include "cx.h"
#include "nvram.h"

const nvram_data N_data;

int get_highest_level() {
    return N_data.highest_level;
}

void write_highest_level(int lvl) {
    nvm_write((void*)&N_data.highest_level, &lvl, sizeof(lvl));
}
