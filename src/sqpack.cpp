#include "sqpack.h"
#include "compression.h"

#include <fmt/format.h>

std::pair<std::string, std::string> Repository::get_index_filenames(const int category) {
    std::string base = fmt::format("{category:02x}{expansion:02d}{chunk:02d}.{platform}",
                   fmt::arg("category", category),
                   fmt::arg("expansion", expansion_number),
                   fmt::arg("chunk", 0),
                   fmt::arg("platform", "win32"));

    return {fmt::format("{}.index", base),
            fmt::format("{}.index2", base)};
}

std::string Repository::get_dat_filename(const int category, const uint32_t data_file_id) {
    return fmt::format("{category:02x}{expansion:02d}{chunk:02d}.{platform}.dat{data_file_id}",
                       fmt::arg("category", category),
                       fmt::arg("expansion", expansion_number),
                       fmt::arg("chunk", 0),
                       fmt::arg("platform", "win32"),
                       fmt::arg("data_file_id", data_file_id));
}

std::vector<std::uint8_t> read_data_block(FILE* file, size_t starting_position) {
    struct BlockHeader {
        int32_t size;
        int32_t dummy;
        int32_t compressedLength; // < 32000 is uncompressed data
        int32_t decompressedLength;
    } header;

    fseek(file, starting_position, SEEK_SET);

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
}