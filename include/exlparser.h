#pragma once

#include <string_view>
#include <vector>
#include <string>
#include "memorybuffer.h"

struct EXLRow {
    std::string name;
    int id;
};

struct EXL {
    std::vector<EXLRow> rows;
};

EXL readEXL(MemorySpan data);