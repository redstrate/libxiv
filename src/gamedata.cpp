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

    auto root_exl_data = extractFile("exd/root.exl");
    rootEXL = readEXL(*root_exl_data);
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

std::optional<MemoryBuffer> GameData::extractFile(const std::string_view data_file_path) {
    const uint64_t hash = calculateHash(data_file_path);
    auto [repository, category] = calculateRepositoryCategory(data_file_path);

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

                return {data};
            } else if(info.fileType == FileType::Model) {
                MemoryBuffer buffer;

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
                uint32_t stackSize = 0;
                uint32_t runtimeSize = 0;

                std::array<uint32_t, 3> vertexDataOffsets;
                std::array<uint32_t, 3> indexDataOffsets;

                std::array<uint32_t, 3> vertexDataSizes;
                std::array<uint32_t, 3> indexDataSizes;

                // data.append 0x44
                buffer.seek(0x44, Seek::Set);

                fseek(file, baseOffset + modelInfo.stackOffset, SEEK_SET);
                size_t stackStart = buffer.current_position();
                for(int i = 0; i < modelInfo.stackBlockNum; i++) {
                    size_t lastPos = ftell(file);

                    auto data = read_data_block(file, lastPos);
                    buffer.write(data);

                    fseek(file, lastPos + compressedBlockSizes[currentBlock], SEEK_SET);
                    currentBlock++;
                }

                size_t stackEnd = buffer.current_position();
                stackSize = (int)(stackEnd - stackStart);

                fseek(file, baseOffset + modelInfo.runtimeOffset, SEEK_SET);
                size_t runtimeStart = buffer.current_position();
                for(int i = 0; i < modelInfo.runtimeBlockNum; i++) {
                    size_t lastPos = ftell(file);

                    auto data = read_data_block(file, lastPos);
                    buffer.write(data);

                    fseek(file, lastPos + compressedBlockSizes[currentBlock], SEEK_SET);
                    currentBlock++;
                }

                size_t runtimeEnd = buffer.current_position();
                runtimeSize = (int)(runtimeEnd - runtimeStart);

                // process all 3 lods
                for(int i = 0; i < 3; i++) {
                    if(modelInfo.vertexBlockBufferBlockNum[i] != 0) {
                        int currentVertexOffset = buffer.current_position();
                        if(i == 0 || currentVertexOffset != vertexDataOffsets[i - 1])
                            vertexDataOffsets[i] = currentVertexOffset;
                        else
                            vertexDataOffsets[i] = 0;

                        fseek(file, baseOffset + modelInfo.vertexBufferOffset[i], SEEK_SET);

                        for(int j = 0; j < modelInfo.vertexBlockBufferBlockNum[i]; j++) {
                            size_t lastPos = ftell(file);

                            auto data = read_data_block(file, lastPos);
                            buffer.write(data);

                            vertexDataSizes[i] += (int)data.size();
                            fseek(file, lastPos + compressedBlockSizes[currentBlock], SEEK_SET);
                            currentBlock++;
                        }
                    }

                    // TODO: lol no edge geometry

                    if(modelInfo.indexBufferBlockNum[i] != 0) {
                        int currentIndexOffset = buffer.current_position();
                        if(i == 0 || currentIndexOffset != indexDataOffsets[i - 1])
                            indexDataOffsets[i] = currentIndexOffset;
                        else
                            indexDataOffsets[i] = 0;

                        for(int j = 0; j < modelInfo.indexBufferBlockNum[i]; j++) {
                            size_t lastPos = ftell(file);

                            auto data = read_data_block(file, lastPos);
                            buffer.write(data);

                            indexDataSizes[i] += (int)data.size();
                            fseek(file, lastPos + compressedBlockSizes[currentBlock], SEEK_SET);
                            currentBlock++;
                        }
                    }
                }

                // now write mdl header
                buffer.seek(0, Seek::Set);

                buffer.write(modelInfo.version);
                buffer.write(stackSize);
                buffer.write(runtimeSize);
                buffer.write(modelInfo.vertexDeclarationNum);
                buffer.write(modelInfo.materialNum);

                for(int i = 0; i < 3; i++)
                    buffer.write(vertexDataOffsets[i]);

                for(int i = 0; i < 3; i++)
                    buffer.write(indexDataOffsets[i]);

                for(int i = 0; i < 3; i++)
                    buffer.write(vertexDataSizes[i]);

                for(int i = 0; i < 3; i++)
                    buffer.write(indexDataSizes[i]);

                buffer.write(modelInfo.numLods);
                buffer.write(modelInfo.indexBufferStreamingEnabled);
                buffer.write(modelInfo.edgeGeometryEnabled);

                uint8_t dummy = 0;
                buffer.write(dummy);

                fclose(file);

                return {buffer};
            } else {
                throw std::runtime_error("File type is not handled yet for " + std::string(data_file_path));
            }
        }
    }

    fmt::print("Failed to find file {}.\n", data_file_path);

    return std::nullopt;
}

bool GameData::exists(std::string_view data_file_path) {
    const uint64_t hash = calculateHash(data_file_path);
    auto [repository, category] = calculateRepositoryCategory(data_file_path);

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
        if (entry.hash == hash) {
            return true;
        }
    }

    return false;
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

            auto exh_data = extractFile(exhFilename);
            return readEXH(*exh_data);
        }
    }

    return {};
}

void GameData::extractSkeleton(Race race) {
    const std::string path = fmt::format("chara/human/c{race:04d}/skeleton/base/b0001/skl_c{race:04d}b0001.sklb",
                                         fmt::arg("race", get_race_id(race)));
    auto skel_data = extractFile(path);
    auto skel_span = MemorySpan(*skel_data);

    int32_t magic;
    skel_span.read(&magic);

    if(magic != 0x736B6C62)
        fmt::print("INVALID SKLB magic");

    int32_t format;
    skel_span.read(&format);

    skel_span.seek(sizeof(uint16_t), Seek::Current);

    int16_t dataOffset = 0;
    switch(format) {
        case 0x31323030:
            skel_span.read(&dataOffset);
            break;
        case 0x31333030:
        case 0x31333031:
            skel_span.seek(sizeof(uint16_t), Seek::Current);
            skel_span.read(&dataOffset);
            break;
        default:
            fmt::print("INVALID SKLB format {}", format);
            break;
    }

    skel_span.seek(dataOffset, Seek::Set);

    std::vector<uint8_t> havokData(skel_span.size() - dataOffset);
    skel_span.read_structures(&havokData, havokData.size());

    const std::string outputName = fmt::format("skl_c{race:04d}b0001.sklb",
                                               fmt::arg("race", get_race_id(race)));

    FILE* newFile = fopen(outputName.c_str(), "wb");
    fwrite(havokData.data(), havokData.size(), 1, newFile);

    fclose(newFile);
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
