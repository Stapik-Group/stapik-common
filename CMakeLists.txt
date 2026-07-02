cmake_minimum_required(VERSION 3.25)
project(stapikcommon LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
        nlohmann_json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(nlohmann_json)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED IMPORTED_TARGET glib-2.0)

add_library(stapikcommon STATIC
        src/stapik/locale/LocalizationEngine.cpp
        src/stapik/cloud/CloudStorageClient.cpp
        src/stapik/storage/AppPaths.cpp
        src/stapik/sync/Timestamp.cpp
        src/stapik/sync/SyncEnvelope.cpp
)

target_include_directories(stapikcommon PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)

target_link_libraries(stapikcommon PUBLIC
        PkgConfig::GLIB
        nlohmann_json::nlohmann_json
)