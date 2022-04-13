#include "havokxmlparser.h"

#include <pugixml.hpp>
#include <fmt/core.h>
#include <iostream>
#include <algorithm>

void walkSkeleton(Skeleton& skeleton, Bone *pBone, int level = 0);

inline std::vector<std::string> tokenize(const std::string_view string, const std::string_view& delimiters) {
    std::vector<std::string> tokens;

    const size_t length = string.length();
    size_t lastPos = 0;

    while(lastPos < length + 1) {
        size_t pos = string.find_first_of(delimiters, lastPos);
        if(pos == std::string_view::npos)
            pos = length;

        if(pos != lastPos)
            tokens.emplace_back(string.data() + lastPos, pos - lastPos);

        lastPos = pos + 1;
    }

    return tokens;
}

Skeleton parseHavokXML(const std::string_view path) {
    Skeleton skeleton;

    pugi::xml_document doc;
    doc.load_file(path.data());

    pugi::xpath_node build_tool = doc.select_node("//hkobject[@name=\"#0052\"]/hkparam[@name=\"bones\"]");

    auto bonesNode = build_tool.node();

    fmt::print("num bones: {}\n", bonesNode.attribute("numelements").as_int());

    skeleton.bones.reserve(bonesNode.attribute("numelements").as_int());

    for(auto node : bonesNode.children()) {
        pugi::xpath_node name_node = node.select_node("./hkparam[@name=\"name\"]");
        //fmt::print("{}\n", node.print());
        name_node.node().print(std::cout);

        Bone bone;
        bone.name = name_node.node().text().as_string();

        fmt::print("bone {}\n", bone.name);

        skeleton.bones.push_back(bone);
    }
    pugi::xpath_node parentNode = doc.select_node("//hkparam[@name=\"parentIndices\"]");

    std::string text = parentNode.node().text().as_string();
    std::replace(text.begin(), text.end(), '\n', ' ');
    text.erase(std::remove(text.begin(), text.end(), '\t'), text.end());

    auto parentIndices = tokenize(text, " ");

    for(int i = 0; i < parentIndices.size(); i++) {
        auto indice = std::atoi(parentIndices[i].c_str());
        if(indice != -1)
            skeleton.bones[i].parent = &skeleton.bones[indice];
        else
            skeleton.root_bone = &skeleton.bones[i];
    }

    walkSkeleton(skeleton, skeleton.root_bone);

    pugi::xpath_node build_tool2 = doc.select_node("//hkobject[@name=\"#0052\"]/hkparam[@name=\"referencePose\"]");

    fmt::print("num ref poses: {}\n", build_tool2.node().attribute("numelements").as_int());

    auto matrices = tokenize(build_tool2.node().text().as_string(), "\n");
    int i = 0;
    for(auto matrix : matrices) {
        size_t firstParenthesis = matrix.find_first_of('(');
        if(firstParenthesis != std::string::npos) {
            matrix = matrix.substr(firstParenthesis);
            std::replace(matrix.begin(), matrix.end(), ')', ' ');
            matrix.erase(std::remove(matrix.begin(), matrix.end(), '('), matrix.end());
            fmt::print("{}\n", matrix);

            auto tokens = tokenize(matrix, " ");
            std::array<float, 3> position = {std::stof(tokens[0]), std::stof(tokens[1]), std::stof(tokens[2])};
            std::array<float, 4> rotation = {std::stof(tokens[3]), std::stof(tokens[4]), std::stof(tokens[5]), std::stof(tokens[6])};
            std::array<float, 3> scale = {std::stof(tokens[7]), std::stof(tokens[8]), std::stof(tokens[9])};

            skeleton.bones[i].position = position;
            skeleton.bones[i].rotation = rotation;
            skeleton.bones[i].scale = scale;
        }

        i++;
    }

    return skeleton;
}

void walkSkeleton(Skeleton& skeleton, Bone *pBone, int level) {
    for(int i = 0; i < level; i++)
        fmt::print(" ");

    fmt::print("- {}\n", pBone->name);

    for(auto& bone : skeleton.bones) {
        if(bone.parent == pBone) {
            walkSkeleton(skeleton, &bone, level++);
        }
    }
}
