#include "exlparser.h"

#include <stdexcept>
#include <fstream>

EXL readEXL(MemorySpan data) {
    auto stream = data.read_as_stream();

    EXL exl;

    std::string line;
    while (std::getline(stream, line)) {
        const size_t comma = line.find_first_of(',');

        std::string name = line.substr(0, comma);

        if(name != "EXLT") {
            std::string id = line.substr(comma + 1, line.length());

            exl.rows.push_back({name, std::stoi(id)});
        }
    }

    return exl;
}