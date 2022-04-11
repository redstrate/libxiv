#pragma once

#include <string_view>
#include <vector>
#include <string>

struct EXLRow {
    std::string name;
    int id;
};

struct EXL {
    std::vector<EXLRow> rows;
};

EXL readEXL(std::string_view path);