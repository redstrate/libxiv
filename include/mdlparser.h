#pragma once

#include <string_view>
#include <vector>
#include <array>

struct Vertex {
    std::array<float, 3> position;
    std::array<float, 3> normal;
};

struct Part {
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
};

struct Lod {
    std::vector<Part> parts;
};

struct Model {
    std::vector<Lod> lods;
};

Model parseMDL(const std::string_view path);