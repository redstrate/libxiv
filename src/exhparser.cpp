#include "exhparser.h"
#include "utility.h"

#include <cstdio>
#include <stdexcept>
#include <vector>
#include <algorithm>
#include <string>

EXH readEXH(MemorySpan data) {
    EXH exh;

    data.read(&exh.header);

    data.seek(0x20, Seek::Set);

    endianSwap(&exh.header.dataOffset);
    endianSwap(&exh.header.columnCount);
    endianSwap(&exh.header.pageCount);
    endianSwap(&exh.header.languageCount);
    endianSwap(&exh.header.rowCount);

    data.read_structures(&exh.columnDefinitions, exh.header.columnCount);
    data.read_structures(&exh.pages, exh.header.pageCount);
    data.read_structures(&exh.language, exh.header.languageCount);

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