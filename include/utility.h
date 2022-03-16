#pragma once

#include <algorithm>

template <class T>
void endianSwap(T *objp) {
    auto memp = reinterpret_cast<unsigned char*>(objp);
    std::reverse(memp, memp + sizeof(T));
}