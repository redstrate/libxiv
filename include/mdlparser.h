#pragma once

#include <string_view>
#include <vector>
#include <array>

#include "memorybuffer.h"

struct Vertex {
    std::array<float, 3> position;
    std::array<float, 2> uv;
    std::array<float, 3> normal;

    std::array<float, 4> boneWeights;
    std::array<uint8_t, 4> boneIds;
};

struct PartSubmesh {
    uint32_t indexOffset, indexCount;
    uint32_t boneStartIndex, boneCount;
};

struct Part {
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;

    std::vector<PartSubmesh> submeshes;
};

struct Lod {
    std::vector<Part> parts;
};

struct Model {
    std::vector<Lod> lods;

    std::vector<std::string> affectedBoneNames;
};

Model parseMDL(MemorySpan data);