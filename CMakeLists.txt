project(libxiv)

include(FetchContent)

find_package(fmt QUIET)

if(TARGET fmt::fmt)
    message("Using system library for fmt")

    set(LIBRARIES fmt::fmt ${LIBRARIES})
else()
    message("Using downloaded fmt")

    FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt
    GIT_TAG        master
    )

    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(fmt)

    set(LIBRARIES fmt::fmt ${LIBRARIES})
endif()

# note: unshield does not work on windows per maintainer notice in README, so we might as well not even attempt to look
# for it.
if(NOT WIN32)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(libunshield REQUIRED libunshield)

    set(LIBRARIES ${libunshield_LIBRARIES} ${LIBRARIES})
    set(LIB_DIRS ${libunshield_LIBRARY_DIRS})
endif()

find_package(ZLIB QUIET)

if(TARGET ZLIB::ZLIB)
    message("Using system library for zlib")

    set(LIBRARIES ZLIB::ZLIB ${LIBRARIES})
else()
    message("Using downloaded zlib")

    FetchContent_Declare(
            zlib
            GIT_REPOSITORY https://github.com/madler/zlib.git
            GIT_TAG        master
    )

    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(zlib)

    # welcome to hell, cmake.
    include_directories(${CMAKE_BINARY_DIR}/_deps/zlib-src)
    # NO REALLY, HELL IS THIS WAY
    include_directories(${CMAKE_BINARY_DIR}/_deps/zlib-build)

    set(LIBRARIES zlibstatic ${LIBRARIES})
endif()

find_package(pugixml QUIET)

if(TARGET pugixml::pugixml)
    message("Using system library for zlib")
else()
    message("Using downloaded pugixml")

    FetchContent_Declare(
    pugixml
    GIT_REPOSITORY https://github.com/zeux/pugixml.git
    GIT_TAG        master
    )

    FetchContent_MakeAvailable(pugixml)
endif()

FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 0.9.9.8
)

FetchContent_MakeAvailable(glm)

add_library(libxiv STATIC
        src/fiinparser.cpp
        src/indexparser.cpp
        src/crc32checksum.cpp
        src/gamedata.cpp
        src/compression.cpp
        src/exhparser.cpp
        src/exdparser.cpp
        src/installextract.cpp
        src/patch.cpp
        src/exlparser.cpp
        src/mdlparser.cpp
        src/havokxmlparser.cpp
        src/types.cpp
        src/equipment.cpp
        src/sqpack.cpp
        src/memorybuffer.cpp)
target_include_directories(libxiv PUBLIC include PRIVATE src)
target_link_libraries(libxiv PUBLIC ${LIBRARIES} pugixml::pugixml glm::glm)
target_link_directories(libxiv PUBLIC ${LIB_DIRS})
target_compile_features(libxiv PUBLIC cxx_std_17)
set_target_properties(libxiv PROPERTIES CXX_EXTENSIONS OFF)

if(NOT WIN32)
    target_compile_definitions(libxiv PUBLIC UNSHIELD_SUPPORTED)
endif()

install(TARGETS libxiv
        DESTINATION "${INSTALL_LIB_PATH}")