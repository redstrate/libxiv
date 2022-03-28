#include "compression.h"

#include <zlib.h>
#include <stdexcept>
#include <string>

// adopted from
// https://github.com/ahom/ffxiv_reverse/blob/312a0af8b58929fab48438aceae8da587be9407f/xiv/utils/src/zlib.cpp#L31
void zlib::no_header_decompress(uint8_t* in, uint32_t in_size, uint8_t* out, uint32_t out_size) {
    z_stream strm = {};
    strm.avail_in = in_size;

    // Init with -15 because we do not have header in this compressed data
    auto ret = inflateInit2(&strm, -15);
    if (ret != Z_OK) {
        throw std::runtime_error("Error at zlib init: " + std::to_string(ret));
    }

    // Set pointers to the right addresses
    strm.next_in = in;
    strm.avail_out = out_size;
    strm.next_out = out;

    // Effectively decompress data
    ret = inflate(&strm, Z_NO_FLUSH);
    if (ret != Z_STREAM_END) {
        throw std::runtime_error("Error at zlib inflate: " + std::to_string(ret));
    }

    // Clean up
    inflateEnd(&strm);
}