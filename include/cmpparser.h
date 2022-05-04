#pragma once

#include "memorybuffer.h"

struct RacialScalingParameter {

};

struct ColorData {
    uint8_t  r, g, b, a;
};

struct CMP {
    std::vector<ColorData> colorPixels;
};

void read_cmp(MemoryBuffer data);
void write_cmp(CMP cmp);