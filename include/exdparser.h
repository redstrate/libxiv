#pragma once

#include <string_view>
#include <string>
#include <vector>

struct EXH;
struct ExcelDataPagination;

struct Column {
    std::string data;
};

struct Row {
    std::vector<Column> data;
};

struct EXD {
    std::vector<Row> rows;
};

EXD readEXD(EXH& exh, ExcelDataPagination& page);