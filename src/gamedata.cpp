#include "gamedata.h"
#include "indexparser.h"
#include "crc32.h"
#include "compression.h"
#include "string_utils.h"

#include <string>
#include <algorithm>
#include <fmt/printf.h>

// TODO: should be enum?
// taken from https://xiv.dev/data-files/sqpack#categories
std::unordered_map<std::string_view, int> categoryToID = {
        {"common", 0},
        {"bgcommon", 1},
        {"bg", 2},
        {"cut", 3},
        {"chara", 4},
        {"shader", 5},
        {"ui", 6},
        {"sound", 7},
        {"vfx", 8},
        {"ui_script", 9},
        {"exd", 10},
        {"game_script", 11},
        {"music", 12},
        {"sqpack_test", 13},
        {"debug", 14},
};

GameData::GameData(const std::string_view dataDirectory) {
    this->dataDirectory = dataDirectory;
}

uint64_t GameData::calculateHash(const std::string_view path) {
    std::string data = toLowercase(path.data());

    auto lastSeperator = data.find_last_of('/');
    const std::string filename = data.substr(lastSeperator + 1, data.length());
    const std::string directory = data.substr(0, lastSeperator);

    uint32_t table[256] = {};
    CRC32::generate_table(table);

    // we actually want JAMCRC, which is just the bitwise not of a regular crc32 hash
    const uint32_t directoryCrc = ~CRC32::update(table, 0, directory.data(), directory.size());
    const uint32_t filenameCrc = ~CRC32::update(table, 0, filename.data(), filename.size());

    return static_cast<uint64_t>(directoryCrc) << 32 | filenameCrc;
}

std::tuple<std::string, std::string> GameData::calculateRepositoryCategory(std::string_view path) {
    std::string repository, category;

    auto tokens = tokenize(path, "/");
    if(stringContains(tokens[1], "ex") && !stringContains(tokens[0], "exd") && !stringContains(tokens[0], "exh")) {
        repository = tokens[1];
    } else {
        repository = "ffxiv";
    }

    category = tokens[0];

    return {repository, category};
}

int getExpansionID(std::string_view repositoryName) {
    if(repositoryName == "ffxiv")
        return 0;

    return std::stoi(std::string(repositoryName.substr(2, 2)));
}

std::string GameData::calculateFilename(const int category, const int expansion, const int chunk, const std::string_view platform, const std::string_view type) {
    return fmt::sprintf("%02x%02x%02x.%s.%s", category, expansion, chunk, platform, type);
}

void GameData::extractFile(std::string_view dataFilePath, std::string_view outPath) {
    const uint64_t hash = calculateHash(dataFilePath);
    auto [repository, category] = calculateRepositoryCategory(dataFilePath);

    fmt::print("repository = {}\n", repository);
    fmt::print("category = {}\n", category);

    // TODO: handle platforms other than win32
    auto indexFilename = calculateFilename(categoryToID[category], getExpansionID(repository), 0, "win32", "index");

    // TODO: handle hashes in index2 files (we can read them but it's not setup yet.)
    auto indexFile = readIndexFile(dataDirectory + "/" + repository + "/" + indexFilename);

    for(const auto entry : indexFile.entries) {
        if(entry.hash == hash) {
            auto dataFilename = calculateFilename(categoryToID[category], getExpansionID(repository), entry.dataFileId, "win32", "dat0");

            FILE* file = fopen((dataDirectory + "/" + repository + "/" + dataFilename).c_str(), "rb");
            if(file == nullptr) {
                throw std::runtime_error("Failed to open data file: " + dataFilename);
            }

            const size_t offset = entry.offset * 0x80;
            fseek(file, offset, SEEK_SET);

            enum FileType : int32_t {
                Empty = 1,
                Standard = 2,
                Model = 3,
                Texture = 4
            };

            struct FileInfo {
                uint32_t size;
                FileType fileType;
                int32_t fileSize;
                uint32_t dummy[2];
                uint32_t numBlocks;
            } info;

            fread(&info, sizeof(FileInfo), 1, file);

            fmt::print("file size = {}\n", info.fileSize);

            if(info.fileType != FileType::Standard) {
                throw std::runtime_error("File type is not handled yet for " + std::string(dataFilePath));
            }

            struct Block {
                int32_t offset;
                int16_t dummy;
                int16_t dummy2;
            };

            std::vector<Block> blocks;

            for(int i = 0; i < info.numBlocks; i++) {
                Block block;
                fread(&block, sizeof(Block), 1, file);

                blocks.push_back(block);
            }

            std::vector<std::uint8_t> data;

            const size_t startingPos = offset + info.size;
            for(auto block : blocks) {
                struct BlockHeader {
                    int32_t size;
                    int32_t dummy;
                    int32_t compressedLength; // < 32000 is uncompressed data
                    int32_t decompressedLength;
                } header;

                fseek(file, startingPos + block.offset, SEEK_SET);

                fread(&header, sizeof(BlockHeader), 1, file);

                std::vector<uint8_t> localdata;

                bool isCompressed = header.compressedLength < 32000;
                if(isCompressed) {
                    localdata.resize(header.decompressedLength);

                    std::vector<uint8_t> compressed_data;
                    compressed_data.resize(header.compressedLength);
                    fread(compressed_data.data(), header.compressedLength, 1, file);

                    zlib::no_header_decompress(reinterpret_cast<uint8_t*>(compressed_data.data()),
                                         compressed_data.size(),
                                         reinterpret_cast<uint8_t*>(localdata.data()),
                                         header.decompressedLength);
                } else {
                    localdata.resize(header.decompressedLength);

                    fread(localdata.data(), header.decompressedLength, 1, file);
                }

                data.insert(data.end(), localdata.begin(), localdata.end());
            }

            fclose(file);

            FILE* newFile = fopen(outPath.data(), "w");
            fwrite(data.data(), data.size(), 1, newFile);
            fclose(newFile);
        }
    }

    fmt::print("Extracted {} to {}\n", dataFilePath, outPath);
}
