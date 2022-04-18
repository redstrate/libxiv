#pragma once

#include <string_view>
#include <string>
#include <optional>
#include "exhparser.h"
#include "exlparser.h"
#include "indexparser.h"
#include "sqpack.h"
#include "memorybuffer.h"

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
    [[nodiscard]]
    std::optional<MemoryBuffer> extractFile(std::string_view data_file_path);

    bool exists(std::string_view data_file_path);

    IndexFile<IndexHashTableEntry> getIndexListing(std::string_view folder);

    void extractSkeleton();

    std::optional<EXH> readExcelSheet(std::string_view name);

    std::vector<std::string> getAllSheetNames();

    /*
     * Calculates a uint64 hash from a given game path.
     */
    uint64_t calculateHash(std::string_view path);

private:
    Repository& getBaseRepository();

    /*
     * Returns the repository, category for a given game path - respectively.
     */
    std::tuple<Repository, std::string> calculateRepositoryCategory(std::string_view path);

    std::string dataDirectory;
    std::vector<Repository> repositories;

    EXL rootEXL;
};

std::vector<std::uint8_t> read_data_block(FILE* file, size_t starting_position);