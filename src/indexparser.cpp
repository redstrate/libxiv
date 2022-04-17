#include "indexparser.h"

#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

template<class T>
void commonParseSqPack(FILE* file, IndexFile<T>& index) {
    fread(&index.packHeader, sizeof(SqPackHeader), 1, file);

    if(strcmp(index.packHeader.magic, "SqPack") != 0) {
        throw std::runtime_error("Invalid sqpack magic.");
    }

    // data starts at size
    fseek(file, index.packHeader.size, SEEK_SET);

    // read index header
    fread(&index.indexHeader, sizeof(SqPackIndexHeader), 1, file);

    if(index.packHeader.version != 1) {
        throw std::runtime_error("Invalid sqpack version.");
    }

    fseek(file, index.indexHeader.indexDataOffset, SEEK_SET);
}

IndexFile<IndexHashTableEntry> readIndexFile(const std::string_view path) {
    FILE* file = fopen(path.data(), "rb");
    if(!file) {
        throw std::runtime_error("Failed to read index file from " + std::string(path.data()));
    }

    IndexFile<IndexHashTableEntry> index;
    commonParseSqPack(file, index);

    uint32_t numEntries = index.indexHeader.indexDataSize / sizeof(IndexHashTableEntry);
    for(uint32_t i = 0; i < numEntries; i++) {
        IndexHashTableEntry entry = {};
        fread(&entry, sizeof(IndexHashTableEntry), 1, file);

        index.entries.push_back(entry);
    }

    return index;
}

IndexFile<Index2HashTableEntry> readIndex2File(const std::string_view path) {
    FILE* file = fopen(path.data(), "rb");
    if(!file) {
        throw std::runtime_error("Failed to read index2 file from " + std::string(path.data()));
    }

    IndexFile<Index2HashTableEntry> index;
    commonParseSqPack(file, index);

    for(int i = 0; i < index.indexHeader.indexDataSize; i++) {
        Index2HashTableEntry entry = {};
        fread(&entry, sizeof entry, 1, file);

        index.entries.push_back(entry);
    }

    return index;
}

CombinedIndexFile read_index_files(std::string_view index_filename, std::string_view index2_filename) {
    CombinedIndexFile final_index_file;

    auto index_parsed = readIndexFile(index_filename);
    auto index2_parsed = readIndex2File(index2_filename);

    for(auto entry : index_parsed.entries) {
        IndexEntry new_entry;
        new_entry.hash = entry.hash;
        new_entry.offset = entry.offset;
        new_entry.dataFileId = entry.dataFileId;

        final_index_file.entries.push_back(new_entry);
    }

    for(auto entry : index2_parsed.entries) {
        IndexEntry new_entry;
        new_entry.hash = entry.hash;
        new_entry.offset = entry.offset;
        new_entry.dataFileId = entry.dataFileId;

        final_index_file.entries.push_back(new_entry);
    }

    return final_index_file;
}