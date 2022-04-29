#include "memorybuffer.h"

void write_buffer_to_file(const MemoryBuffer& buffer, std::string_view path) {
    FILE* file = fopen(path.data(), "wb");
    if(file == nullptr)
        throw std::runtime_error("Failed to open file for writing.");

    fwrite(buffer.data.data(), buffer.data.size(), 1, file);

    fclose(file);
}

MemoryBuffer read_file_to_buffer(std::string_view path) {
    FILE* file = fopen(path.data(), "rb");
    if(file == nullptr)
        throw std::runtime_error("Failed to open file for reading.");

    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);

    MemoryBuffer buffer;
    buffer.data.resize(size);
    fread(buffer.data.data(), size, 1, file);

    fclose(file);

    return buffer;
}