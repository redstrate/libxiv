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

std::string getEXDFilename(EXH& exh, std::string_view name, std::string_view lang, ExcelDataPagination& page);

EXD readEXD(EXH& exh, std::string_view path, ExcelDataPagination& page);