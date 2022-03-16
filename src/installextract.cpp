#include "installextract.h"

#include <cstdio>
#include <stdexcept>
#include <libunshield.h>
#include <fmt/format.h>
#include <filesystem>

const std::array filesToExtract = {"data1.cab", "data1.hdr", "data2.cab"};

const std::array bootComponent = {
        "cef_license.txt",
        "FFXIV.ico",
        "ffxivboot.exe",
        "ffxivboot.ver",
        "ffxivboot64.exe",
        "ffxivconfig.exe",
        "ffxivconfig64.exe",
        "ffxivlauncher.exe",
        "ffxivlauncher64.exe",
        "ffxivsysinfo.exe",
        "ffxivsysinfo64.exe",
        "ffxivupdater.exe",
        "ffxivupdater64.exe",
        "FFXIV_sysinfo.ico",
        "icudt.dll",
        "libcef.dll",
        "license.txt",
        "locales/reserved.txt"
};

const std::array gameComponent = {
        "ffxivgame.ver"
};

void extractBootstrapFiles(const std::string_view installer, const std::string_view directory) {
    FILE* file = fopen(installer.data(), "rb");
    if(file == nullptr) {
        throw std::runtime_error("Failed to open installer {}" + std::string(installer));
    }

    fseek(file, 0, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    auto buffer = static_cast<char*>(malloc(sizeof(char) * fileSize));

    fread(buffer, 1, fileSize, file);

    // some really, really bad code to basically read the cab files which are put right next to each other in the exe file
    char* lastNeedle = nullptr;
    std::string lastFilename;
    for(auto fileName : filesToExtract) {
        // all files are located in Disk1
        const std::string realFileName = fmt::format("Disk1\\{}", fileName);
        const char* needle = realFileName.c_str();

        // make unshield shut up
        unshield_set_log_level(0);

        // TODO: is this while really needed? it should only find it once
        while(true) {
            char* p = static_cast<char*>(memmem(buffer, fileSize, needle, realFileName.length()));
            if(!p)
                break;

            if(lastNeedle != nullptr) {
                FILE* newFile = fopen(lastFilename.data(), "wb");
                fmt::print("Wrote {}!\n", lastFilename);

                if(lastFilename == "data1.hdr") {
                    fwrite(lastNeedle + 30, p - lastNeedle - 42, 1, newFile);
                } else {
                    fwrite(lastNeedle + 32, p - lastNeedle - 42, 1, newFile);
                }

                // actual cab files start 32 bytes and we need to peel 42 bytes off of the end
                fclose(newFile);
            }

            lastFilename = fileName;
            lastNeedle = p;
            fileSize -= (p + 4) - buffer;
            buffer = p + realFileName.length();
        }

        // write final file
        FILE* newFile = fopen(lastFilename.data(), "wb");
        fmt::print("Wrote {}!\n", lastFilename);
        fwrite(lastNeedle + 33, fileSize - 42, 1, newFile);

        // actual cab files start 32 bytes and we need to peel 42 bytes off of the end
        fclose(newFile);
    }

    if(!std::filesystem::exists(directory)) {
        fmt::print("Game install directory doesn't exist yet, creating...\n");
        std::filesystem::create_directories(directory);
    }

    if(!std::filesystem::exists(std::filesystem::path(directory) / "boot")) {
        fmt::print("Boot directory doesn't exist yet, creating...\n");
        std::filesystem::create_directory(std::filesystem::path(directory) / "boot");
    }

    if(!std::filesystem::exists(std::filesystem::path(directory) / "game")) {
        fmt::print("Game directory doesn't exist yet, creating...\n");
        std::filesystem::create_directory(std::filesystem::path(directory) / "game");
    }

    auto unshield = unshield_open("data1.cab");
    const int fileCount = unshield_file_count(unshield);

    for(int i = 0 ; i < fileCount; i++) {
        const char* filename = unshield_file_name(unshield, i);

        for(auto bootName : bootComponent) {
            fmt::print("Extracted {}\n", bootName);

            if(strcmp(filename, bootName) == 0) {
                std::string saveFilename = fmt::format("{}/boot/{}", directory, bootName);
                unshield_file_save(unshield, i, saveFilename.data());
            }
        }

        for(auto gameName : gameComponent) {
            fmt::print("Extracted {}\n", gameName);

            if(strcmp(filename, gameName) == 0) {
                std::string saveFilename = fmt::format("{}/game/{}", directory, gameName);
                unshield_file_save(unshield, i, saveFilename.data());
            }
        }
    }

    fclose(file);
}