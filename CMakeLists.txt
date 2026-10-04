cmake_minimum_required(VERSION 4.2)
project(stapikcommon VERSION 1.2.1 LANGUAGES CXX)

set(stapikcommon_VERSION_SUFFIX "-dev")
set(stapikcommon_VERSION_FULL "${stapikcommon_VERSION}_${stapikcommon_VERSION_SUFFIX}")
message(STATUS "Building: ${PROJECT_NAME} version: ${stapikcommon_VERSION_FULL}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(STAPIK_COMMON_BUILD_TESTS "Build the stapik-common unit tests" OFF)
option(STAPIK_COMMON_INSTALL_PACKAGE
        "Install the library, its headers and the CMake package files (off by default when used through FetchContent)"
        ${PROJECT_IS_TOP_LEVEL})

include(cmake/StapikApp.cmake)

include(FetchContent)
if(STAPIK_COMMON_INSTALL_PACKAGE)
    set(JSON_Install ON)
endif()
FetchContent_Declare(
        nlohmann_json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
        FIND_PACKAGE_ARGS 3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTKMM REQUIRED IMPORTED_TARGET gtkmm-4.0)
pkg_check_modules(SIGCPP REQUIRED IMPORTED_TARGET sigc++-3.0)
find_package(CURL REQUIRED)

add_library(stapikcommon STATIC)
add_library(stapik::common ALIAS stapikcommon)
set_target_properties(stapikcommon PROPERTIES EXPORT_NAME common)

target_sources(stapikcommon PRIVATE
        src/stapik/app/AppContext.cpp
)

target_sources(stapikcommon PRIVATE
        src/stapik/cloud/CloudStorageClient.cpp
)

target_sources(stapikcommon PRIVATE
        src/stapik/command/CompositeCommand.cpp
        src/stapik/command/UndoStack.cpp
)

target_sources(stapikcommon PRIVATE
        src/stapik/document/DocumentFileCore.cpp
        src/stapik/document/SchemaMigrator.cpp
)

target_sources(stapikcommon PRIVATE
        src/stapik/domain/CategoryColor.cpp
        src/stapik/domain/CurrencyCatalog.cpp
        src/stapik/domain/IdGenerator.cpp
        src/stapik/domain/Money.cpp
        src/stapik/domain/MoneyFormatter.cpp
        src/stapik/domain/YearMonth.cpp
)

target_sources(stapikcommon PRIVATE
        src/stapik/locale/LanguageRegistry.cpp
        src/stapik/locale/LocaleManager.cpp
        src/stapik/locale/LocalizationEngine.cpp
        src/stapik/locale/SystemLanguage.cpp
)

target_sources(stapikcommon PRIVATE
        src/stapik/log/Log.cpp
)

target_sources(stapikcommon PRIVATE
        src/stapik/settings/AppSettings.cpp
        src/stapik/settings/SettingsStore.cpp
)

target_sources(stapikcommon PRIVATE
        src/stapik/storage/AppPaths.cpp
        src/stapik/storage/AtomicFile.cpp
        src/stapik/storage/CloudStorageConfigStorage.cpp
)

target_sources(stapikcommon PRIVATE
        src/stapik/sync/CloudSessionHooks.cpp
        src/stapik/sync/SyncEnvelope.cpp
        src/stapik/sync/Timestamp.cpp
)

target_sources(stapikcommon PRIVATE
        src/stapik/task/BackgroundTaskRunner.cpp
        src/stapik/task/DebouncedAction.cpp
        src/stapik/task/MainThread.cpp
)

target_sources(stapikcommon PRIVATE
        src/stapik/theme/Theme.cpp
        src/stapik/theme/ThemeManager.cpp
        src/stapik/theme/ThemeRegistry.cpp
)

target_sources(stapikcommon PRIVATE
        src/stapik/ui/dialog/AboutDialog.cpp
        src/stapik/ui/dialog/ChoiceDialog.cpp
        src/stapik/ui/dialog/ConfirmDialog.cpp
        src/stapik/ui/dialog/ConnectDialog.cpp
        src/stapik/ui/dialog/DialogUtils.cpp
        src/stapik/ui/dialog/StapikDialog.cpp
        src/stapik/ui/dialog/TextInputDialog.cpp
        src/stapik/ui/menu/RadioAction.cpp
        src/stapik/ui/menu/StandardMenu.cpp
        src/stapik/ui/menu/UndoActions.cpp
        src/stapik/ui/style/AppStyleProvider.cpp
        src/stapik/ui/widget/StatusIndicator.cpp
)

stapik_enable_warnings(stapikcommon)

target_compile_definitions(stapikcommon PRIVATE
        STAPIK_COMMON_SOURCE_RESOURCES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/resources"
)

include(GNUInstallDirs)

target_compile_features(stapikcommon PUBLIC cxx_std_20)

target_include_directories(stapikcommon PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(stapikcommon PUBLIC
        PkgConfig::GTKMM
        PkgConfig::SIGCPP
        nlohmann_json::nlohmann_json
        CURL::libcurl
)

set(STAPIK_COMMON_RESOURCES_INSTALL_DIR "${CMAKE_INSTALL_DATADIR}/stapik-common" CACHE STRING
        "Where the stapik-common resources (locales, themes) are installed, relative to the install prefix")
install(DIRECTORY resources/ DESTINATION "${STAPIK_COMMON_RESOURCES_INSTALL_DIR}")

set(STAPIK_COMMON_RESOURCES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/resources" CACHE INTERNAL
        "Directory with the stapik-common resources")

if(STAPIK_COMMON_INSTALL_PACKAGE)
    include(CMakePackageConfigHelpers)

    set(STAPIK_COMMON_CMAKE_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/stapikcommon")

    install(TARGETS stapikcommon
            EXPORT stapikcommonTargets
            ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
    install(DIRECTORY src/stapik DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}" FILES_MATCHING PATTERN "*.hpp")
    install(EXPORT stapikcommonTargets
            NAMESPACE stapik::
            FILE stapikcommon-targets.cmake
            DESTINATION "${STAPIK_COMMON_CMAKE_INSTALL_DIR}")

    configure_package_config_file(
            cmake/stapikcommon-config.cmake.in
            "${CMAKE_CURRENT_BINARY_DIR}/stapikcommon-config.cmake"
            INSTALL_DESTINATION "${STAPIK_COMMON_CMAKE_INSTALL_DIR}"
            PATH_VARS STAPIK_COMMON_RESOURCES_INSTALL_DIR)
    write_basic_package_version_file(
            "${CMAKE_CURRENT_BINARY_DIR}/stapikcommon-config-version.cmake"
            VERSION "${PROJECT_VERSION}"
            COMPATIBILITY SameMajorVersion)

    install(FILES
            "${CMAKE_CURRENT_BINARY_DIR}/stapikcommon-config.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/stapikcommon-config-version.cmake"
            cmake/StapikApp.cmake
            cmake/StapikWarnings.cmake
            DESTINATION "${STAPIK_COMMON_CMAKE_INSTALL_DIR}")
    install(DIRECTORY cmake/templates DESTINATION "${STAPIK_COMMON_CMAKE_INSTALL_DIR}")
endif()

if(STAPIK_COMMON_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
