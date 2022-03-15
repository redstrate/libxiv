#pragma once

#include <algorithm>

std::vector<std::string> tokenize(const std::string_view string, const std::string_view& delimiters) {
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

bool stringContains(const std::string_view a, const std::string_view b) {
    return a.find(b) != std::string::npos;
}

std::string toLowercase(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(),
                   [](unsigned char c){ return std::tolower(c); });

    return str;
}