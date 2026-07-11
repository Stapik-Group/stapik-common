cmake_minimum_required(VERSION 4.2)
project(stapikcommon VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
        nlohmann_json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(nlohmann_json)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTKMM REQUIRED IMPORTED_TARGET gtkmm-4.0)
pkg_check_modules(SIGCPP REQUIRED IMPORTED_TARGET sigc++-3.0)
find_package(CURL REQUIRED)

add_library(stapikcommon STATIC
        src/stapik/locale/LocalizationEngine.cpp
        src/stapik/locale/LocaleManager.cpp
        src/stapik/cloud/CloudStorageClient.cpp
        src/stapik/storage/AppPaths.cpp
        src/stapik/storage/CloudStorageConfigStorage.cpp
        src/stapik/sync/Timestamp.cpp
        src/stapik/sync/SyncEnvelope.cpp
        src/stapik/ui/dialog/DialogUtils.cpp
        src/stapik/ui/dialog/ConnectDialog.cpp
        src/stapik/ui/style/AppStyleProvider.cpp
)

target_include_directories(stapikcommon PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)

target_link_libraries(stapikcommon PUBLIC
        PkgConfig::GTKMM
        PkgConfig::SIGCPP
        nlohmann_json::nlohmann_json
        CURL::libcurl
)