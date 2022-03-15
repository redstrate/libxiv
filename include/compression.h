#pragma once

#include <cstdint>

namespace zlib {
    void no_header_decompress(uint8_t* in, uint32_t in_size, uint8_t* out, uint32_t out_size);
}