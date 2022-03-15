project(libxiv)

add_library(libxiv STATIC
        src/fiinparser.cpp
        src/indexparser.cpp
        src/crc32.cpp
        src/gamedata.cpp
        src/compression.cpp)
target_include_directories(libxiv PUBLIC include PRIVATE src)