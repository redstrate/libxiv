#include "crc32checksum.h"

void CRC32::generate_table(uint32_t(&table)[256]) {
    uint32_t polynomial = 0xEDB88320;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (size_t j = 0; j < 8; j++) {
            if (c & 1) {
                c = polynomial ^ (c >> 1);
            }
            else {
                c >>= 1;
            }
        }
        table[i] = c;
    }
}

uint32_t CRC32::update(uint32_t (&table)[256], uint32_t initial, const void* buf, size_t len) {
    uint32_t c = initial ^ 0xFFFFFFFF;
    const auto* u = static_cast<const uint8_t*>(buf);
    for (size_t i = 0; i < len; ++i) {
        c = table[(c ^ u[i]) & 0xFF] ^ (c >> 8);
    }

    return c ^ 0xFFFFFFFF;
}