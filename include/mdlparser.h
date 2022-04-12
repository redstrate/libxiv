#pragma once

#include <string_view>
#include <vector>
#include <array>

struct Vertex {
    std::array<float, 4> position;
    float blendWeights[4];
    std::vector<uint8_t> blendIndices;
    float normal[3];
    float uv[4];
    float color[4];
    float tangent2[4];
    float tangent1[4];
};

struct Model {
};

Model parseMDL(const std::string_view path);