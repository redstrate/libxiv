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
#include <filesystem>

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

    for(auto const& dir_entry : std::filesystem::directory_iterator{dataDirectory}) {
        if(!dir_entry.is_directory())
            continue;

        Repository repository;
        repository.name = dir_entry.path().filename().string();
        repository.type = stringContains(repository.name, "ex") ? Repository::Type::Expansion : Repository::Type::Base;

        if(repository.type == Repository::Type::Expansion)
            repository.expansion_number = std::stoi(repository.name.substr(2));

        repositories.push_back(repository);
    }

    extractFile("exd/root.exl", "root.exl");

    rootEXL = readEXL("root.exl");
}

std::vector<std::string> GameData::getAllSheetNames() {
    std::vector<std::string> names;

    for(auto& row : rootEXL.rows)
        names.push_back(row.name);

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

std::tuple<Repository, std::string> GameData::calculateRepositoryCategory(std::string_view path) {
    const auto tokens = tokenize(path, "/");
    const std::string repositoryToken = tokens[0];

    for(auto& repository : repositories) {
        if(repository.name == repositoryToken) {
            // if this is an expansion, the next token is the category
            return {repository, tokens[1]};
        }
    }

    // if it doesn't match any existing repositories (in the case of accessing base game data),
    // fall back to base repository.
    return {getBaseRepository(), tokens[0]};
}

void GameData::extractFile(std::string_view dataFilePath, std::string_view outPath) {
    const uint64_t hash = calculateHash(dataFilePath);
    auto [repository, category] = calculateRepositoryCategory(dataFilePath);

    auto [index_filename, index2_filename] = repository.get_index_filenames(categoryToID[category]);
    auto index_path = fmt::format("{data_directory}/{repository}/{filename}",
                                  fmt::arg("data_directory", dataDirectory),
                                  fmt::arg("repository", repository.name),
                                  fmt::arg("filename", index_filename));
    auto index2_path = fmt::format("{data_directory}/{repository}/{filename}",
                                  fmt::arg("data_directory", dataDirectory),
                                  fmt::arg("repository", repository.name),
                                  fmt::arg("filename", index2_filename));

    auto index_file = read_index_files(index_path, index2_path);

    for(const auto entry : index_file.entries) {
        if(entry.hash == hash) {
            auto data_filename = repository.get_dat_filename(categoryToID[category], entry.dataFileId);

            FILE* file = fopen((dataDirectory + "/" + repository.name + "/" + data_filename).c_str(), "rb");
            if(file == nullptr) {
                throw std::runtime_error("Failed to open data file: " + data_filename);
            }

            const size_t offset = entry.offset * 0x80;
            fseek(file, offset, SEEK_SET);

            struct FileInfo {
                uint32_t size;
                FileType fileType;
                int32_t fileSize;
                uint32_t dummy[2];
                uint32_t numBlocks;
            } info;

            fread(&info, sizeof(FileInfo), 1, file);

            struct Block {
                int32_t offset;
                int16_t dummy;
                int16_t dummy2;
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
                    auto localdata = read_data_block(file, startingPos + block.offset);
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
                    auto data = read_data_block(file, lastPos);
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
                    auto data = read_data_block(file, lastPos);
                    fwrite(data.data(), data.size(), 1, newFile);
                    fseek(file, lastPos + compressedBlockSizes[currentBlock], SEEK_SET);
                    currentBlock++;
                }

                size_t runtimeEnd = ftell(newFile);
                runtimeSize = (int)(runtimeEnd - runtimeStart);

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
                            auto data = read_data_block(file, lastPos);
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
                            auto data = read_data_block(file, lastPos);
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

                fclose(newFile);
                fclose(file);
            } else {
                throw std::runtime_error("File type is not handled yet for " + std::string(dataFilePath));
            }

            fmt::print("Extracted {} to {}!\n", dataFilePath, outPath);

            return;
        }
    }

    fmt::print("Failed to find file {}.\n", dataFilePath);
}

std::optional<EXH> GameData::readExcelSheet(std::string_view name) {
    for(const auto& row : rootEXL.rows) {
        if(row.name == name) {
            // we want it as lowercase (Item -> item)
            std::string newFilename = name.data();
            std::transform(newFilename.begin(), newFilename.end(), newFilename.begin(),
                           [](unsigned char c){ return std::tolower(c); });

            std::string exhFilename = "exd/" + newFilename + ".exh";

            std::string outPath = newFilename + ".exh";
            std::replace(outPath.begin(), outPath.end(), '/', '_');

            extractFile(exhFilename, outPath);

            return readEXH(outPath);
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

    std::vector<uint8_t> havokData(end - dataOffset);
    fread(havokData.data(), havokData.size(), 1, file);

    FILE* newFile = fopen("test.sklb.havok", "wb");
    fwrite(havokData.data(), havokData.size(), 1, newFile);

    fclose(newFile);
    fclose(file);
}

IndexFile<IndexHashTableEntry> GameData::getIndexListing(std::string_view folder) {
    auto [repository, category] = calculateRepositoryCategory(fmt::format("{}/{}", folder, "a"));

    auto [indexFilename, index2Filename] = repository.get_index_filenames(categoryToID[category]);

    return readIndexFile(dataDirectory + "/" + repository.name + "/" + indexFilename);
}

Repository& GameData::getBaseRepository() {
    for(auto& repository : repositories) {
        if(repository.type == Repository::Type::Base)
            return repository;
    }

    throw std::runtime_error("No base repository found.");
}
