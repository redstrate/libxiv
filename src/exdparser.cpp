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
T readDataRaw(MemorySpan& span, int offset) {
    span.seek(offset, Seek::Set);

    T value;
    span.read(&value);
    endianSwap(&value);

    return value;
}

template<typename T>
std::string readData(MemorySpan& span, int offset) {
    return std::to_string(readDataRaw<T>(span, offset));
}

std::string getEXDFilename(EXH& exh, std::string_view name, std::string_view lang, ExcelDataPagination& page) {
    if(lang.empty()) {
        return fmt::format("{}_{}.exd", name, page.startId);
    } else {
        return fmt::format("{}_{}_{}.exd", name, page.startId, lang);
    }
}

EXD readEXD(EXH& exh, MemorySpan data, ExcelDataPagination& page) {
    EXD exd;

    ExcelDataHeader header;
    data.read(&header);

    endianSwap(&header.indexSize);

    std::vector<ExcelDataOffset> dataOffsets;
    data.read_structures(&dataOffsets, header.indexSize / sizeof(ExcelDataOffset));

    for(auto& offset : dataOffsets) {
        endianSwap(&offset.offset);
        endianSwap(&offset.rowId);
    }

    for(int i = 0; i < exh.header.rowCount; i++) {
        for(auto& offset : dataOffsets) {
            if (offset.rowId == i) {
                data.seek(offset.offset, Seek::Set);

                ExcelDataRowHeader rowHeader;
                data.read(&rowHeader);

                endianSwap(&rowHeader.dataSize);
                endianSwap(&rowHeader.rowCount);

                const int headerOffset = offset.offset + 6;

                const auto readRow = [&exd, &exh, &data, rowHeader](int offset) {
                    Row row;

                    for (auto column: exh.columnDefinitions) {
                        Column c;

                        switch (column.type) {
                            case String: {
                                data.seek(offset + column.offset, Seek::Set);

                                uint32_t stringLength = 0; // this is actually offset?
                                data.read(&stringLength);

                                endianSwap(&stringLength);

                                data.seek(offset + exh.header.dataOffset + stringLength, Seek::Set);

                                std::string string;

                                uint8_t byte;
                                data.read(&byte);
                                while(byte != 0) {
                                    string.push_back(byte);
                                    data.read(&byte);
                                }

                                c.data = string;
                                c.type = "String";
                            }
                                break;
                            case Int8:
                                c.data = readData<int8_t>(data, offset + column.offset);
                                c.type = "Int";
                                c.uint64Data = readDataRaw<int8_t>(data, offset + column.offset);
                                break;
                            case UInt8:
                                c.data = readData<uint8_t>(data, offset + column.offset);
                                c.type = "Unsigned Int";
                                c.uint64Data = readDataRaw<uint8_t>(data, offset + column.offset);
                                break;
                            case Int16:
                                c.data = readData<int16_t>(data, offset + column.offset);
                                c.type = "Int";
                                c.uint64Data = readDataRaw<int16_t>(data, offset + column.offset);
                                break;
                            case UInt16:
                                c.data = readData<uint16_t>(data, offset + column.offset);
                                c.type = "Unsigned Int";
                                break;
                            case Int32:
                                c.data = readData<int32_t>(data, offset + column.offset);
                                c.type = "Int";
                                c.uint64Data = readDataRaw<int32_t>(data, offset + column.offset);
                                break;
                            case UInt32:
                                c.data = readData<uint32_t>(data, offset + column.offset);
                                c.type = "Unsigned Int";
                                break;
                            case Float32:
                                c.data = readData<float>(data, offset + column.offset);
                                c.type = "Float";
                                break;
                            case Int64:
                                c.data = readData<int64_t>(data, offset + column.offset);
                                c.type = "Int";
                                c.uint64Data = readDataRaw<int64_t>(data, offset + column.offset);
                                break;
                            case UInt64:
                                c.data = readData<uint64_t>(data, offset + column.offset);
                                c.type = "Unsigned Int";
                                c.uint64Data = readDataRaw<uint64_t>(data, offset + column.offset);
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
                                int32_t boolData = readDataRaw<int32_t>(data, offset + column.offset);
                                c.data = std::to_string((boolData & bit) == bit);
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