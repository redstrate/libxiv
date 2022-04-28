#pragma once

#include <string>
#include <vector>
#include <array>
#include <glm/glm.hpp>

struct Bone {
    std::string name;

    Bone* parent = nullptr;

    glm::mat4 localTransform, finalTransform, inversePose;

    std::array<float, 3> position;
    std::array<float, 4> rotation;
    std::array<float, 3> scale;
};

struct Skeleton {
    std::vector<Bone> bones;
    Bone* root_bone = nullptr;
};

/*
 * This reads a havok xml scene file, which is generated from your preferred assetcc.exe.
 */
Skeleton parseHavokXML(const std::string_view path);