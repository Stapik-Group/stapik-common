cmake_minimum_required(VERSION 4.2)
project(stapikcommon VERSION 1.2.0 LANGUAGES CXX)

set(stapikcommon_VERSION_SUFFIX "-dev")
set(stapikcommon_VERSION_FULL "${stapikcommon_VERSION}_${stapikcommon_VERSION_SUFFIX}")
message(STATUS "Building: ${PROJECT_NAME} version: ${stapikcommon_VERSION_FULL}")

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
        src/stapik/app/AppContext.cpp
        src/stapik/document/DocumentFileCore.cpp
        src/stapik/document/SchemaMigrator.cpp
        src/stapik/locale/LanguageRegistry.cpp
        src/stapik/locale/LocalizationEngine.cpp
        src/stapik/locale/SystemLanguage.cpp
        src/stapik/locale/LocaleManager.cpp
        src/stapik/cloud/CloudStorageClient.cpp
        src/stapik/log/Log.cpp
        src/stapik/settings/AppSettings.cpp
        src/stapik/settings/SettingsStore.cpp
        src/stapik/storage/AppPaths.cpp
        src/stapik/storage/AtomicFile.cpp
        src/stapik/storage/CloudStorageConfigStorage.cpp
        src/stapik/sync/Timestamp.cpp
        src/stapik/sync/SyncEnvelope.cpp
        src/stapik/ui/dialog/DialogUtils.cpp
        src/stapik/ui/dialog/ConnectDialog.cpp
        src/stapik/ui/style/AppStyleProvider.cpp
        src/stapik/theme/Theme.cpp
        src/stapik/theme/ThemeManager.cpp
        src/stapik/theme/ThemeRegistry.cpp
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

option(STAPIK_COMMON_BUILD_TESTS "Build the stapik-common unit tests" OFF)
if(STAPIK_COMMON_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
