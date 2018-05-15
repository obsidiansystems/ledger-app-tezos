#pragma once

typedef struct {
    int highest_level;
} nvram_data;
extern WIDE nvram_data N_data_real;
#define N_data (*(WIDE nvram_data*)PIC(&N_data_real))

int get_highest_level();
void write_highest_level(int lvl);
