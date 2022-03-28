#pragma once

#include <cstdint>
#include <cstddef>

// adapted from https://gist.github.com/timepp/1f678e200d9e0f2a043a9ec6b3690635
namespace CRC32 {
    void generate_table(uint32_t(&table)[256]);
    uint32_t update(uint32_t (&table)[256], uint32_t initial, const void* buf, size_t len);
}