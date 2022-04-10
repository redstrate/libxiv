#include "exhparser.h"
#include "utility.h"

#include <cstdio>
#include <stdexcept>
#include <vector>
#include <algorithm>
#include <string>

EXH readEXH(const std::string_view path) {
    EXH exh;

    FILE* file = fopen(path.data(), "rb");
    if(file == nullptr) {
        throw std::runtime_error("Failed to open exh file " + std::string(path));
    }

    fread(&exh.header, sizeof(ExhHeader), 1, file);

    endianSwap(&exh.header.dataOffset);
    endianSwap(&exh.header.columnCount);
    endianSwap(&exh.header.pageCount);
    endianSwap(&exh.header.languageCount);
    endianSwap(&exh.header.rowCount);

    exh.columnDefinitions.resize(exh.header.columnCount);

    fread(exh.columnDefinitions.data(), sizeof(ExcelColumnDefinition) * exh.header.columnCount, 1, file);

    exh.pages.resize(exh.header.pageCount);

    fread(exh.pages.data(), sizeof(ExcelDataPagination) * exh.header.pageCount, 1, file);

    exh.language.resize(exh.header.languageCount);
    fread(exh.language.data(), sizeof(Language) * exh.header.languageCount, 1, file);

    for(auto& columnDef : exh.columnDefinitions) {
        endianSwap(&columnDef.offset);
        endianSwap(&columnDef.type);
    }

    for(auto& page : exh.pages) {
        endianSwap(&page.rowCount);
        endianSwap(&page.startId);
    }

    return exh;
}