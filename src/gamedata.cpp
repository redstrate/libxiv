#include "gamedata.h"
#include "indexparser.h"
#include "crc32checksum.h"
#include "compression.h"
#include "string_utils.h"
#include "exlparser.h"

#include <string>
#include <algorithm>
#include <unordered_map>
#include <array>
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

    extractFile("exd/root.exl", "root.exl");

    rootEXL = readEXL("root.exl");
}

std::vector<std::string> GameData::getAllSheetNames() {
    std::vector<std::string> names;

    for(auto row : rootEXL.rows) {
        names.push_back(row.name);
    }

    return names;
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
    if(type == "index") {
        return fmt::sprintf("%02x%02x%02x.%s.%s", category, expansion, chunk, platform, type);
    } else if(type == "dat") {
        return fmt::sprintf("%02x%02x00.%s.%s%01x", category, expansion, platform, type, chunk);
    }
}

void GameData::extractFile(std::string_view dataFilePath, std::string_view outPath) {
    const uint64_t hash = calculateHash(dataFilePath);
    auto [repository, category] = calculateRepositoryCategory(dataFilePath);

    fmt::print("repository = {}\n", repository);
    fmt::print("category = {}\n", category);

    // TODO: handle platforms other than win32
    auto indexFilename = calculateFilename(categoryToID[category], getExpansionID(repository), 0, "win32", "index");

    fmt::print("calculated index filename: {}\n", indexFilename);

    // TODO: handle hashes in index2 files (we can read them but it's not setup yet.)
    auto indexFile = readIndexFile(dataDirectory + "/" + repository + "/" + indexFilename);

    for(const auto entry : indexFile.entries) {
        if(entry.hash == hash) {
            auto dataFilename = calculateFilename(categoryToID[category], getExpansionID(repository), entry.dataFileId, "win32", "dat");

            fmt::print("Opening data file {}...\n", dataFilename);

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

            struct Block {
                int32_t offset;
                int16_t dummy;
                int16_t dummy2;
            };

            const auto readFileBlock = [](FILE* file, size_t startingPos) -> std::vector<std::uint8_t> {
                struct BlockHeader {
                    int32_t size;
                    int32_t dummy;
                    int32_t compressedLength; // < 32000 is uncompressed data
                    int32_t decompressedLength;
                } header;

                fseek(file, startingPos, SEEK_SET);

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

                return localdata;
            };

            if(info.fileType == FileType::Standard) {
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

            } else if(info.fileType == FileType::Model) {
                FILE* newFile = fopen(outPath.data(), "w");

                // reset
                fseek(file, offset, SEEK_SET);

                struct ModelFileInfo {
                    uint32_t size;
                    FileType fileType;
                    uint32_t fileSize;
                    uint32_t numBlocks;
                    uint32_t numUsedBlocks;
                    uint32_t version;
                    uint32_t stackSize;
                    uint32_t runtimeSize;
                    uint32_t vertexBufferSize[3];
                    uint32_t edgeGeometryVertexBufferSize[3];
                    uint32_t indexBufferSize[3];
                    uint32_t compressedStackMemorySize;
                    uint32_t compressedRuntimeMemorySize;
                    uint32_t compressedVertexBufferSize[3];
                    uint32_t compressedEdgeGeometrySize[3];
                    uint32_t compressedIndexBufferSize[3];
                    uint32_t stackOffset;
                    uint32_t runtimeOffset;
                    uint32_t vertexBufferOffset[3];
                    uint32_t edgeGeometryVertexBufferOffset[3];
                    uint32_t indexBufferOffset[3];
                    uint16_t stackBlockIndex;
                    uint16_t runtimeBlockIndex;
                    uint16_t vertexBufferBlockIndex[3];
                    uint16_t edgeGeometryVertexBufferBlockIndex[3];
                    uint16_t indexBufferBlockIndex[3];
                    uint16_t stackBlockNum;
                    uint16_t runtimeBlockNum;
                    uint16_t vertexBlockBufferBlockNum[3];
                    uint16_t edgeGeometryVertexBufferBlockNum[3];
                    uint16_t indexBufferBlockNum[3];
                    uint16_t vertexDeclarationNum;
                    uint16_t materialNum;
                    uint8_t numLods;
                    bool indexBufferStreamingEnabled;
                    bool edgeGeometryEnabled;
                    uint8_t padding;
                } modelInfo;

                fread(&modelInfo, sizeof(ModelFileInfo), 1, file);

                const size_t baseOffset = offset + modelInfo.size;

                int totalBlocks = modelInfo.stackBlockNum;
                totalBlocks += modelInfo.runtimeBlockNum;
                for(int i = 0; i < 3; i++) {
                    totalBlocks += modelInfo.vertexBlockBufferBlockNum[i];
                    totalBlocks += modelInfo.edgeGeometryVertexBufferBlockNum[i];
                    totalBlocks += modelInfo.indexBufferBlockNum[i];
                }

                std::vector<uint16_t> compressedBlockSizes(totalBlocks);
                fread(compressedBlockSizes.data(), compressedBlockSizes.size() * sizeof(uint16_t), 1, file);
                int currentBlock = 0;
                int stackSize = 0;
                int runtimeSize = 0;

                std::array<int, 3> vertexDataOffsets;
                std::array<int, 3> indexDataOffsets;

                std::array<int, 3> vertexDataSizes;
                std::array<int, 3> indexDataSizes;

                // data.append 0x44
                fseek(newFile, 0x44, SEEK_SET);

                fseek(file, baseOffset + modelInfo.stackOffset, SEEK_SET);
                size_t stackStart = ftell(newFile);
                for(int i = 0; i < modelInfo.stackBlockNum; i++) {
                    size_t lastPos = ftell(file);
                    auto data = readFileBlock(file, lastPos);
                    fwrite(data.data(), data.size(), 1, newFile); // i think we write this to file?
                    fseek(file, lastPos + compressedBlockSizes[currentBlock], SEEK_SET);
                    currentBlock++;
                }

                size_t stackEnd = ftell(newFile);
                stackSize = (int)(stackEnd - stackStart);

                fseek(file, baseOffset + modelInfo.runtimeOffset, SEEK_SET);
                size_t runtimeStart = ftell(newFile);
                for(int i = 0; i < modelInfo.runtimeBlockNum; i++) {
                    size_t lastPos = ftell(file);
                    auto data = readFileBlock(file, lastPos);
                    fwrite(data.data(), data.size(), 1, newFile);
                    fseek(file, lastPos + compressedBlockSizes[currentBlock], SEEK_SET);
                    currentBlock++;
                }

                size_t runtimeEnd = ftell(newFile);
                runtimeSize = (int)(runtimeEnd - runtimeStart);

                fmt::print("stack size: {}\n", stackSize);
                fmt::print("runtime size: {}\n", runtimeSize);

                // process all 3 lods
                for(int i = 0; i < 3; i++) {
                    if(modelInfo.vertexBlockBufferBlockNum[i] != 0) {
                        int currentVertexOffset = ftell(newFile);
                        if(i == 0 || currentVertexOffset != vertexDataOffsets[i - 1])
                            vertexDataOffsets[i] = currentVertexOffset;
                        else
                            vertexDataOffsets[i] = 0;

                        fseek(file, baseOffset + modelInfo.vertexBufferOffset[i], SEEK_SET);

                        for(int j = 0; j < modelInfo.vertexBlockBufferBlockNum[i]; j++) {
                            size_t lastPos = ftell(file);
                            auto data = readFileBlock(file, lastPos);
                            fwrite(data.data(), data.size(), 1, newFile); // i think we write this to file?
                            vertexDataSizes[i] += (int)data.size();
                            fseek(file, lastPos + compressedBlockSizes[currentBlock], SEEK_SET);
                            currentBlock++;
                        }
                    }

                    // TODO: lol no edge geometry

                    if(modelInfo.indexBufferBlockNum[i] != 0) {
                        int currentIndexOffset = ftell(newFile);
                        if(i == 0 || currentIndexOffset != indexDataOffsets[i - 1])
                            indexDataOffsets[i] = currentIndexOffset;
                        else
                            indexDataOffsets[i] = 0;

                        for(int j = 0; j < modelInfo.indexBufferBlockNum[i]; j++) {
                            size_t lastPos = ftell(file);
                            auto data = readFileBlock(file, lastPos);
                            fwrite(data.data(), data.size(), 1, newFile); // i think we write this to file?
                            indexDataSizes[i] += (int)data.size();
                            fseek(file, lastPos + compressedBlockSizes[currentBlock], SEEK_SET);
                            currentBlock++;
                        }
                    }
                }

                // now write mdl header
                fseek(newFile, 0, SEEK_SET);
                fwrite(&modelInfo.version, sizeof(uint32_t), 1, newFile);
                fwrite(&stackSize, sizeof(uint32_t), 1, newFile);
                fwrite(&runtimeSize, sizeof(uint32_t), 1, newFile);
                fwrite(&modelInfo.vertexDeclarationNum, sizeof(unsigned short), 1, newFile);
                fwrite(&modelInfo.materialNum, sizeof(unsigned short), 1, newFile);

                for(int i = 0; i < 3; i++)
                    fwrite(&vertexDataOffsets[i], sizeof(uint32_t), 1, newFile);

                for(int i = 0; i < 3; i++)
                    fwrite(&indexDataOffsets[i], sizeof(uint32_t), 1, newFile);

                for(int i = 0; i < 3; i++)
                    fwrite(&vertexDataSizes[i], sizeof(uint32_t), 1, newFile);

                for(int i = 0; i < 3; i++)
                    fwrite(&indexDataSizes[i], sizeof(uint32_t), 1, newFile);

                fwrite(&modelInfo.numLods, sizeof(uint8_t), 1, file);
                fwrite(&modelInfo.indexBufferStreamingEnabled, sizeof(bool), 1, file);
                fwrite(&modelInfo.edgeGeometryEnabled, sizeof(bool), 1, file);

                uint8_t dummy[] = {0};
                fwrite(dummy, sizeof(uint8_t), 1, file);

                fmt::print("data size: {}\n", modelInfo.fileSize);

                fclose(newFile);
                fclose(file);
            } else {
                throw std::runtime_error("File type is not handled yet for " + std::string(dataFilePath));
            }
        }
    }

    fmt::print("Extracted {} to {}\n", dataFilePath, outPath);
}

std::optional<EXH> GameData::readExcelSheet(std::string_view name) {
    fmt::print("Beginning to read excel sheet {}...\n", name);

    for(auto row : rootEXL.rows) {
        if(row.name == name) {
            fmt::print("Found row {} at id {}!\n", name, row.id);

            // we want it as lowercase (Item -> item)
            std::string newFilename = name.data();
            std::transform(newFilename.begin(), newFilename.end(), newFilename.begin(),
                           [](unsigned char c){ return std::tolower(c); });

            std::string exhFilename = "exd/" + newFilename + ".exh";

            extractFile(exhFilename, newFilename + ".exh");

            fmt::print("Done extracting files, now parsing...\n");

            return readEXH(newFilename + ".exh");
        }
    }

    return {};
}

void GameData::extractSkeleton() {
    std::string path = fmt::format("chara/human/c0201/skeleton/base/b0001/skl_c0201b0001.sklb");

    extractFile(path, "test.skel");

    FILE* file = fopen("test.skel", "rb");

    fseek(file, 0, SEEK_END);
    size_t end = ftell(file);
    fseek(file, 0, SEEK_SET);

    int32_t magic;
    fread(&magic, sizeof(int32_t), 1, file);

    int32_t format;
    fread(&format, sizeof(int32_t), 1, file);

    fseek(file, sizeof(uint16_t), SEEK_CUR);

    if(magic != 0x736B6C62)
        fmt::print("INVALID SKLB magic");

    size_t dataOffset = 0;

    switch(format) {
        case 0x31323030:
            fread(&dataOffset, sizeof(int16_t), 1, file);
            break;
        case 0x31333030:
        case 0x31333031:
            fseek(file, sizeof(uint16_t), SEEK_CUR);
            fread(&dataOffset, sizeof(int16_t), 1, file);
            break;
        default:
            fmt::print("INVALID SKLB format {}", format);
            break;
    }

    fseek(file, dataOffset, SEEK_SET);

    fmt::print("data offset: {}\n", dataOffset);

    std::vector<uint8_t> havokData(end - dataOffset);
    fread(havokData.data(), havokData.size(), 1, file);

    FILE* newFile = fopen("test.sklb.havok", "wb");
    fwrite(havokData.data(), havokData.size(), 1, newFile);

    fclose(newFile);
    fclose(file);
}
