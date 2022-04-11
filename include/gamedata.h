#pragma once

#include <string_view>
#include <string>
#include <optional>
#include "exhparser.h"
#include "exlparser.h"

/*
 * This handles reading/extracting the raw data from game data packs, such as dat0, index and index2 files.
 * This is not local to "one" repository or sqpack, but oversees operation over all of them.
 *
 * This will "lazy-load" index and dat files as needed for now.
 *
 * This is definitely not the final name of this class :-p
 */
class GameData {
public:
    /*
     * Initializes the game data manager, this should pointing to the parent directory of the ex1/ex2/ffxiv directory.
     */
    explicit GameData(std::string_view dataDirectory);

    /*
     * This extracts the raw file from dataFilePath to outPath;
     */
    void extractFile(std::string_view dataFilePath, std::string_view outPath);

    std::optional<EXH> readExcelSheet(std::string_view name);

    std::vector<std::string> getAllSheetNames();

private:
    /*
     * This returns a proper SQEX-style filename for index, index2, and dat files.
     * filenames are in the format of {category}{expansion}{chunk}.{platform}.{type}
     */
    std::string calculateFilename(int category, int expansion, int chunk, std::string_view platform, std::string_view type);

    /*
     * Returns the repository, category for a given game path - respectively.
     */
    std::tuple<std::string, std::string> calculateRepositoryCategory(std::string_view path);

    /*
     * Calculates a uint64 hash from a given game path.
     */
    uint64_t calculateHash(std::string_view path);

    std::string dataDirectory;

    EXL rootEXL;
};