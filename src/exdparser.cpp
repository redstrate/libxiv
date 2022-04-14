#include "exdparser.h"

#include <cstdio>
#include <stdexcept>
#include <algorithm>
#include <fmt/format.h>

#include "exhparser.h"
#include "utility.h"

struct ExcelDataHeader {
    char magic[4];
    uint16_t version;
    uint16_t unknown1;
    uint32_t indexSize;
    uint16_t unknown2[10];
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
T readDataRaw(FILE* file, int offset) {
    fseek(file, offset, SEEK_SET);

    T value;
    fread(&value, sizeof value, 1, file);
    endianSwap(&value);
    return value;
}

template<typename T>
std::string readData(FILE* file, int offset) {
    return std::to_string(readDataRaw<T>(file, offset));
}

std::string getEXDFilename(EXH& exh, std::string_view name, std::string_view lang, ExcelDataPagination& page) {
    if(lang.empty()) {
        return fmt::format("{}_{}.exd", name, page.startId);
    } else {
        return fmt::format("{}_{}_{}.exd", name, page.startId, lang);
    }
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
    dataOffsets.resize(header.indexSize / sizeof(ExcelDataOffset));

    fread(dataOffsets.data(), sizeof(ExcelDataOffset) * dataOffsets.size(), 1, file);

    for(auto& offset : dataOffsets) {
        endianSwap(&offset.offset);
        endianSwap(&offset.rowId);
    }

    for(int i = 0; i < exh.header.rowCount; i++) {
        for(auto& offset : dataOffsets) {
            if (offset.rowId == i) {
                fseek(file, offset.offset, SEEK_SET);

                ExcelDataRowHeader rowHeader;
                fread(&rowHeader, sizeof(ExcelDataRowHeader), 1, file);

                endianSwap(&rowHeader.dataSize);
                endianSwap(&rowHeader.rowCount);

                const int headerOffset = offset.offset + 6;

                const auto readRow = [&exd, &exh, file, rowHeader](int offset) {
                    Row row;

                    for (auto column: exh.columnDefinitions) {
                        Column c;

                        switch (column.type) {
                            case String: {
                                fseek(file, offset + column.offset, SEEK_SET);

                                uint32_t stringLength = 0; // this is actually offset?
                                fread(&stringLength, sizeof(uint32_t), 1, file);

                                endianSwap(&stringLength);

                                fseek(file, offset + exh.header.dataOffset + stringLength, SEEK_SET);

                                std::string string;

                                uint8_t byte;
                                fread(&byte, sizeof(uint8_t), 1, file);
                                while(byte != 0) {
                                    string.push_back(byte);
                                    fread(&byte, sizeof(uint8_t), 1, file);
                                }

                                c.data = string;
                                c.type = "String";
                            }
                                break;
                            case Int8:
                                c.data = readData<int8_t>(file, offset + column.offset);
                                c.type = "Int";
                                c.uint64Data = readDataRaw<int8_t>(file, offset + column.offset);
                                break;
                            case UInt8:
                                c.data = readData<uint8_t>(file, offset + column.offset);
                                c.type = "Unsigned Int";
                                c.uint64Data = readDataRaw<uint8_t>(file, offset + column.offset);
                                break;
                            case Int16:
                                c.data = readData<int16_t>(file, offset + column.offset);
                                c.type = "Int";
                                c.uint64Data = readDataRaw<int16_t>(file, offset + column.offset);
                                break;
                            case UInt16:
                                c.data = readData<uint16_t>(file, offset + column.offset);
                                c.type = "Unsigned Int";
                                break;
                            case Int32:
                                c.data = readData<int32_t>(file, offset + column.offset);
                                c.type = "Int";
                                c.uint64Data = readDataRaw<int32_t>(file, offset + column.offset);
                                break;
                            case UInt32:
                                c.data = readData<uint32_t>(file, offset + column.offset);
                                c.type = "Unsigned Int";
                                break;
                            case Float32:
                                c.data = readData<float>(file, offset + column.offset);
                                c.type = "Float";
                                break;
                            case Int64:
                                c.data = readData<int64_t>(file, offset + column.offset);
                                c.type = "Int";
                                c.uint64Data = readDataRaw<int64_t>(file, offset + column.offset);
                                break;
                            case UInt64:
                                c.data = readData<uint64_t>(file, offset + column.offset);
                                c.type = "Unsigned Int";
                                c.uint64Data = readDataRaw<uint64_t>(file, offset + column.offset);
                                break;
                            case PackedBool0:
                            case PackedBool1:
                            case PackedBool2:
                            case PackedBool3:
                            case PackedBool4:
                            case PackedBool5:
                            case PackedBool6:
                            case PackedBool7: {
                                int shift = (int) column.type - (int) PackedBool0;
                                int bit = 1 << shift;
                                int32_t data = readDataRaw<int32_t>(file, offset + column.offset);
                                c.data = std::to_string((data & bit) == bit);
                                c.type = "Boolean";
                            }
                                break;
                            default:
                                c.data = "undefined";
                                c.type = "Unknown";
                                break;
                        }

                        row.data.push_back(c);
                    }

                    exd.rows.push_back(row);
                };

                if (rowHeader.rowCount > 1) {
                    for (int i = 0; i < rowHeader.rowCount; i++) {
                        int subrowOffset = headerOffset + (i * exh.header.dataOffset + 2 * (i + 1));
                        readRow(subrowOffset);
                    }
                } else {
                    readRow(headerOffset);
                }
            }
        }
    }

    return exd;
}