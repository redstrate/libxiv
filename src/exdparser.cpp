#include "exdparser.h"

#include <cstdio>
#include <stdexcept>
#include <algorithm>
#include <fmt/format.h>

#include "exhparser.h"
#include "utility.h"

struct ExcelDataHeader {
    char magic[0x4];
    uint16_t version;
    uint16_t unknown1;
    uint32_t indexSize;
    uint32_t unknown2[5];
};

struct ExcelDataOffset {
    uint32_t rowId;
    uint32_t offset;
};

struct ExcelDataRowHeader {
    uint32_t dataSize;
    uint16_t rowCount;
};

template<typename T>
std::string readData(FILE* file, int offset) {
    fseek(file, offset, SEEK_SET);

    T value;
    fread(&value, sizeof value, 1, file);
    endianSwap(&value);
    return std::to_string(value);
}

std::string getEXDFilename(EXH& exh, std::string_view name, ExcelDataPagination& page) {
    auto path = fmt::format("{}_{}.exd", name, page.startId);

    return path;
}

EXD readEXD(EXH& exh, std::string_view path, ExcelDataPagination& page) {
    EXD exd;

    FILE* file = fopen(path.data(), "rb");
    if(file == nullptr) {
        throw std::runtime_error("Failed to open exd file " + std::string(path));
    }

    ExcelDataHeader header;
    fread(&header, sizeof(ExcelDataHeader), 1, file);

    endianSwap(&header.indexSize);

    std::vector<ExcelDataOffset> dataOffsets;
    dataOffsets.resize(header.indexSize / sizeof( ExcelDataOffset ));

    fread(dataOffsets.data(), sizeof(ExcelDataOffset) * dataOffsets.size(), 1, file);

    for(auto& offset : dataOffsets) {
        endianSwap(&offset.offset);
        endianSwap(&offset.rowId);
    }

    for(auto& offset : dataOffsets) {
        Row row;

        fseek(file, exh.header.dataOffset + offset.offset, SEEK_SET);

        ExcelDataRowHeader rowHeader;
        fread(&rowHeader, sizeof(ExcelDataRowHeader), 1, file);

        endianSwap(&rowHeader.dataSize);
        endianSwap(&rowHeader.rowCount);

        const int rowOffset = offset.offset + 6;

        for(auto column : exh.columnDefinitions) {
            Column c;

            switch(column.type) {
                case String:
                {
                    fseek(file, rowOffset + column.offset, SEEK_SET);

                    uint32_t stringLength;
                    fread(&stringLength, sizeof(uint32_t), 1, file);

                    fseek(file, rowOffset + exh.header.dataOffset + stringLength, SEEK_SET);

                    std::string string;

                    int ch;
                    while ((ch = fgetc(file)) != '\0' && ch != '\377') {
                        string.push_back((char)ch);
                    }

                    fmt::print("{}\n", string.data());

                    c.data = string;
                }
                break;
                case Int8:
                    c.data = readData<int8_t>(file, rowOffset + column.offset);
                    break;
                case UInt8:
                    c.data = readData<uint8_t>(file, rowOffset + column.offset);
                    break;
                case Int16:
                    c.data = readData<int16_t>(file, rowOffset + column.offset);
                    break;
                case UInt16:
                    c.data = readData<uint16_t>(file, rowOffset + column.offset);
                    break;
                case Int32:
                    c.data = readData<int32_t>(file, rowOffset + column.offset);
                    break;
                case UInt32:
                    c.data = readData<uint32_t>(file, rowOffset + column.offset);
                    break;
                case Float32:
                    c.data = readData<float>(file, rowOffset + column.offset);
                    break;
                case Int64:
                    c.data = readData<int64_t>(file, rowOffset + column.offset);
                    break;
                case UInt64:
                    c.data = readData<uint64_t>(file, rowOffset + column.offset);
                    break;
                default:
                    c.data = "undefined";
                    break;
            }

            row.data.push_back(c);
        }

        exd.rows.push_back(row);
    }

    return exd;
}