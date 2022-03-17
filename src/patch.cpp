#include "patch.h"

#include <stdexcept>
#include <cstdio>
#include <fmt/core.h>

#include "utility.h"
#include "compression.h"

// based off of https://xiv.dev/data-files/zipatch and some parts of xivquicklauncher
// since the docs are incomplete
// 24 bytes long
struct FHDRChunk {
    uint16_t dum;
    uint8_t version; // 3?
    uint8_t dum2;
    char name[4];
    uint8_t dummy[8]; // TODO: what are these? xiv.dev says they're unused
    uint32_t depotHash;
};

struct APLYChunk {
    uint32_t option;
    uint32_t reserved;
    uint32_t value;
};

struct ADIRChunk {
    uint32_t pathLength;
    std::string path;
};

struct DELDChunk {
    uint32_t pathLength;
    std::string path;
};

void processPatch(const std::string_view path, const std::string_view installDirectory) {
    FILE* file = fopen(path.data(), "rb");
    if(file == nullptr) {
        throw std::runtime_error("Failed to open patch file " + std::string(path));
    }

    // TODO: check signature
    fseek(file, 12, SEEK_SET);

    // TODO: UGLY CODE
    while(true) {
        uint32_t chunksize;
        fread(&chunksize, sizeof(uint32_t), 1, file);
        endianSwap(&chunksize);

        char chunkname[4];
        fread(chunkname, 4, 1, file);

        if(strncmp(chunkname, "FHDR", 4) == 0) {
            fmt::print("FHDR\n");

            FHDRChunk chunk;
            fread(&chunk, sizeof(FHDRChunk), 1, file);

            endianSwap(&chunk.version); // these still dont work lol
            endianSwap(&chunk.depotHash);

            if(strncmp(chunk.name, "HIST", 4) == 0)
                fmt::print("HIST patch!\n");
            else if(strncmp(chunk.name, "DIFF", 4) == 0)
                fmt::print("DIFF patch!\n");
        }

        if(strncmp(chunkname, "APLY", 4) == 0) {
            fmt::print("APLY\n");

            APLYChunk chunk;
            fread(&chunk, sizeof(APLYChunk), 1, file);
        }

        if(strncmp(chunkname, "ADIR", 4) == 0) {
            fmt::print("ADIR\n");

            ADIRChunk chunk;
            fread(&chunk.pathLength, sizeof(uint32_t), 1, file);

            endianSwap(&chunk.pathLength);

            chunk.path.resize(chunk.pathLength);
            fread(chunk.path.data(), chunk.pathLength, 1, file);

            fmt::print("ADIR {}\n", chunk.path);
        }

        if(strncmp(chunkname, "DELD", 4) == 0) {
            fmt::print("DELD\n");

            DELDChunk chunk;
            fread(&chunk.pathLength, sizeof(uint32_t), 1, file);

            endianSwap(&chunk.pathLength);

            chunk.path.resize(chunk.pathLength);
            fread(chunk.path.data(), chunk.pathLength, 1, file);

            fmt::print("DELD {}\n", chunk.path);
        }

        if(strcmp(chunkname, "SQPK") == 0) {
            fmt::print("SQPK\n");

            int32_t hsize;
            fread(&hsize, sizeof(int32_t), 1, file);
            endianSwap(&hsize);

            char opcode;
            fread(&opcode, sizeof(char), 1, file);

            switch(opcode) {
            case 'A':
                fmt::print("A\n");
                break;
            case 'D':
                fmt::print("D\n");
                break;
            case 'E':
                fmt::print("E\n");
                break;
            case 'F': {
                size_t size = hsize - 5;
                size_t start = ftell(file);

                char opcode;
                fread(&opcode, sizeof(char), 1, file);

                // alignment?
                fseek(file, 2, SEEK_CUR);

                int64_t fileOffset;
                fread(&fileOffset, sizeof(int64_t), 1, file);
                endianSwap(&fileOffset);

                uint64_t fileSize;
                fread(&fileSize, sizeof(uint64_t), 1, file);
                endianSwap(&fileSize);

                uint32_t pathLength;
                fread(&pathLength, sizeof(uint32_t), 1, file);
                endianSwap(&pathLength);

                uint32_t expansionId;
                fread(&expansionId, sizeof(uint16_t), 1, file);
                endianSwap(&expansionId);

                // wtf is this
                fseek(file, 2, SEEK_CUR);

                std::string path;
                path.resize(pathLength);
                fread(path.data(), pathLength, 1, file);

                // TODO: implement R, D, and M
                switch(opcode) {
                    case 'A':
                    {
                        std::vector<std::uint8_t> data;

                        size_t currentOffset = ftell(file);
                        while(size - currentOffset + start > 0) {
                            struct BlockHeader {
                                int32_t size;
                                uint32_t dummy;
                                int32_t compressedLength; // < 32000 is uncompressed data
                                int32_t decompressedLength;
                            } header;

                            fread(&header, sizeof(BlockHeader), 1, file);

                            std::vector<uint8_t> localdata;

                            bool isCompressed = header.compressedLength != 32000;
                            int compressedBlockLengtrh = (int)(((isCompressed ? header.compressedLength : header.decompressedLength) + 143) & 0xFFFFFF80);
                            if(isCompressed) {
                                localdata.resize(header.decompressedLength);

                                std::vector<uint8_t> compressed_data;
                                compressed_data.resize(compressedBlockLengtrh - header.size);
                                fread(compressed_data.data(), compressed_data.size(), 1, file);

                                zlib::no_header_decompress(reinterpret_cast<uint8_t*>(compressed_data.data()),
                                                           compressed_data.size(),
                                                           reinterpret_cast<uint8_t*>(localdata.data()),
                                                           localdata.size());
                            } else {
                                localdata.resize(header.decompressedLength);

                                fread(localdata.data(), localdata.size(), 1, file);

                                fseek(file, compressedBlockLengtrh - header.size - header.decompressedLength, SEEK_CUR);
                            }

                            data.insert(data.end(), localdata.begin(), localdata.end());
                            currentOffset = ftell(file);
                        }

                        // TODO: create directory tree
                        std::string newFilename = fmt::format("{}/{}", installDirectory, path);
                        FILE* newFile = fopen(newFilename.c_str(), "w");
                        fwrite(data.data(), data.size(), 1, newFile);
                        fclose(newFile);
                    }
                        break;
                    default:
                        fmt::print("unhandled opcode {}\n", opcode);
                        break;
                }

                fmt::print("F\n");
            }
                break;
            case 'H':
                fmt::print("H\n");
                break;
            case 'I':
                fmt::print("I\n");
                break;
            case 'X': {
                size_t size = hsize - 5;
                size_t start = ftell(file);

                char status;
                fread(&status, sizeof(char), 1, file);

                char version;
                fread(&version, sizeof(char), 1, file);

                fseek(file, 1, SEEK_CUR);

                uint64_t installSize;
                fread(&installSize, sizeof(uint64_t), 1, file);
                endianSwap(&installSize);

                int offset = size - (ftell(file) - start);
                fseek(file, offset, SEEK_CUR);
            }
                break;
            case 'T': {
                size_t size = hsize - 5;
                size_t start = ftell(file);

                // reserved
                fseek(file, 3, SEEK_CUR);

                uint16_t platform;
                fread(&platform, sizeof(uint16_t), 1, file);
                endianSwap(&platform);

                int16_t region;
                fread(&region, sizeof(int16_t), 1, file);
                endianSwap(&region);

                int16_t isDebug;
                fread(&isDebug, sizeof(int16_t), 1, file);
                endianSwap(&isDebug);

                uint16_t version;
                fread(&version, sizeof(uint16_t), 1, file);
                endianSwap(&version);

                uint64_t deletedDataSize;
                fread(&version, sizeof(uint64_t), 1, file);

                uint64_t seekCount;
                fread(&seekCount, sizeof(uint64_t), 1, file);

                int offset = size - (ftell(file) - start);
                fseek(file, offset, SEEK_CUR);
                int newOffset = ftell(file);
                newOffset = newOffset;
                fseek(file, 0, SEEK_CUR);

                // TODO: check if these values actually correct?
                fmt::print("T {} {} {} {} {} {}\n", platform, region, isDebug, version, deletedDataSize, seekCount);
            }
                break;
            default:
                return;
            }
        }

        if(strcmp(chunkname, "EOF_") == 0) {
            fmt::print("EOF\n");
            break;
        }

        fseek(file, sizeof(uint32_t), SEEK_CUR); // crc32, used for something?
    }
}