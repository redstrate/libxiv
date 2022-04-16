#include "exlparser.h"

#include <stdexcept>
#include <fstream>

EXL readEXL(std::string_view path) {
    std::fstream file;
    file.open(path.data(), std::iostream::in);

    if(!file.is_open()) {
        throw std::runtime_error("Failed to read exl file from " + std::string(path.data()));
    }

    EXL exl;

    std::string line;
    while (std::getline(file, line)) {
        const size_t comma = line.find_first_of(',');

        std::string name = line.substr(0, comma);

        if(name != "EXLT") {
            std::string id = line.substr(comma + 1, line.length());

            exl.rows.push_back({name, std::stoi(id)});
        }
    }

    return exl;
}