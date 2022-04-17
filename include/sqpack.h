#pragma once

#include <cstdint>
#include <string>
#include <vector>

// The type of file inside of a SqPack dat file.
// Standard is everything that isn't covered by Model or Texture, such as exd files.
enum class FileType : int32_t {
    Empty = 1,
    Standard = 2,
    Model = 3,
    Texture = 4
};

// This is a folder containing game data, usually seperated by ffxiv (which is always present), and then exX
// where X is the expansion number.
struct Repository {
    enum class Type {
        Base,
        Expansion
    } type = Type::Base;

    std::string name;
    int expansion_number = 0;

    std::pair<std::string, std::string> get_index_filenames(int category);
    std::string get_dat_filename(int category, uint32_t data_file_id);
};