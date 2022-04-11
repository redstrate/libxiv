#include "exlparser.h"
#include <stdexcept>

EXL readEXL(std::string_view path) {
    FILE* file = fopen(path.data(), "rb");
    if(!file) {
        throw std::runtime_error("Failed to read exl file from " + std::string(path.data()));
    }

    EXL exl;

    char* data = nullptr;
    size_t len = 0;

    while ((getline(&data, &len, file)) != -1) {
        std::string line = data;

        const size_t comma = line.find_first_of(',');

        std::string name = line.substr(0, comma);
        std::string id = line.substr(comma + 1, line.length());

        exl.rows.push_back({name, std::stoi(id)});
    }

    return exl;
}