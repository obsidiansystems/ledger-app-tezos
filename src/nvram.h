#pragma once

typedef struct {
    int highest_level;
} nvram_data;
extern const nvram_data N_data;

int get_highest_level();
void write_highest_level(int lvl);
