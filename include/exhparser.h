#pragma once

#include <string_view>
#include <vector>

// taken from https://xiv.dev/game-data/file-formats/excel
struct ExhHeader {
    char magic[0x4];
    uint16_t unknown;
    uint16_t dataOffset;
    uint16_t columnCount;
    uint16_t pageCount;
    uint16_t languageCount;
    uint16_t unknown1;
    uint8_t u2;
    uint8_t variant;
    uint16_t u3;
    uint32_t rowCount;
    uint32_t u4[2];
};

enum ExcelColumnDataType : uint16_t {
    String = 0x0,
    Bool = 0x1,
    Int8 = 0x2,
    UInt8 = 0x3,
    Int16 = 0x4,
    UInt16 = 0x5,
    Int32 = 0x6,
    UInt32 = 0x7,
    Float32 = 0x9,
    Int64 = 0xA,
    UInt64 = 0xB,

    // 0 is read like data & 1, 1 is like data & 2, 2 = data & 4, etc...
    PackedBool0 = 0x19,
    PackedBool1 = 0x1A,
    PackedBool2 = 0x1B,
    PackedBool3 = 0x1C,
    PackedBool4 = 0x1D,
    PackedBool5 = 0x1E,
    PackedBool6 = 0x1F,
    PackedBool7 = 0x20,
};

struct ExcelColumnDefinition {
    ExcelColumnDataType type;
    uint16_t offset;
};

struct ExcelDataPagination {
    uint32_t startId;
    uint32_t rowCount;
};

enum Language : uint16_t {
    None,
    // ja
    Japanese,
    // en
    English,
    // de
    German,
    // fr
    French,
    // chs
    ChineseSimplified,
    // cht
    ChineseTraditional,
    // ko
    Korean
};

struct EXH {
    ExhHeader header;

    std::vector<ExcelColumnDefinition> columnDefinitions;
    std::vector<ExcelDataPagination> pages;
    std::vector<Language> language;
};

EXH readEXH(std::string_view path);