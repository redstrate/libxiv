#pragma once

#include <algorithm>

template <class T>
void endianSwap(T *objp) {
    auto memp = reinterpret_cast<uint8_t*>(objp);
    std::reverse(memp, memp + sizeof(T));
}

// ported from lumina.halfextensions
static float half_to_float(const uint16_t value) {
    unsigned int num3;
    if ((value & -33792) == 0) {
        if ((value & 0x3ff) != 0) {
            auto num2 = 0xfffffff2;
            auto num = (unsigned int) (value & 0x3ff);
            while ((num & 0x400) == 0) {
                num2--;
                num <<= 1;
            }

            num &= 0xfffffbff;
            num3 = ((unsigned int) (value & 0x8000) << 0x10) | ((num2 + 0x7f) << 0x17) | (num << 13);
        } else {
            num3 = (unsigned int) ((value & 0x8000) << 0x10);
        }
    } else {
        num3 = (unsigned int)
                (((value & 0x8000) << 0x10) |
                 ((((value >> 10) & 0x1f) - 15 + 0x7f) << 0x17) |
                 ((value & 0x3ff) << 13));
    }

    return *(float *)&num3;
}

static float byte_to_float(const uint8_t value) {
    return static_cast<float>(value) / 255.0f;
}