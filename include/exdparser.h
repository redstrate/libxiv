#pragma once

#include <string_view>
#include <string>
#include <vector>

#include "memorybuffer.h"

struct EXH;
struct ExcelDataPagination;

struct Column {
    std::string data;
    std::string type; // for debug
    int64_t uint64Data = 0;
};

struct Row {
    std::vector<Column> data;
};

struct EXD {
    std::vector<Row> rows;
};

std::string getEXDFilename(EXH& exh, std::string_view name, std::string_view lang, ExcelDataPagination& page);

EXD readEXD(EXH& exh, MemorySpan data, ExcelDataPagination& page);