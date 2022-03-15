#include "fiinparser.h"

#include <cstdio>
#include <cstring>
#include <fmt/format.h>

FileInfo readFileInfo(const std::string_view path) {
    FILE* file = fopen(path.data(), "rb");
    if(!file) {
        throw std::runtime_error("Failed to read file info from " + std::string(path.data()));
    }

    FileInfo info;
    fread(&info.header, sizeof info.header, 1, file);

    char magic[9] = "FileInfo";
    if(strcmp(info.header.magic, magic) != 0) {
        throw std::runtime_error("Invalid fileinfo magic!");
    }

    int overflow = info.header.unknown2;
    int extra = overflow * 256;
    int first = info.header.unknown1 / 96;
    int first2 = extra / 96;
    int actualEntries = first + first2 + 1; // is this 1 really needed? lol

    int numEntries = actualEntries;
    for(int i = 0; i < numEntries; i++) {
        FileInfoEntry entry;
        fread(&entry, sizeof entry, 1, file);

        info.entries.push_back(entry);
    }

    fclose(file);

    return info;
}