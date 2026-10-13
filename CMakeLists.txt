cmake_minimum_required(VERSION 4.2)
project(stapikcommon VERSION 1.5.3 LANGUAGES CXX)

set(stapikcommon_VERSION_SUFFIX "")
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
find_package(ZLIB REQUIRED)
pkg_check_modules(SODIUM REQUIRED IMPORTED_TARGET libsodium)
if(UNIX AND NOT APPLE)
    # Window position on X11 (see ui/window/WindowPlacement.cpp); without it the position is not remembered
    pkg_check_modules(X11 IMPORTED_TARGET x11)
    # Secret Service client (see storage/SecretStore.cpp); without it secrets cannot be kept in the system keyring
    pkg_check_modules(LIBSECRET IMPORTED_TARGET libsecret-1)
    if(NOT LIBSECRET_FOUND)
        message(WARNING "libsecret-1 not found: the cloud API key will be stored in a plain file")
    endif()
endif()

add_library(stapikcommon STATIC)
add_library(stapik::common ALIAS stapikcommon)
set_target_properties(stapikcommon PROPERTIES EXPORT_NAME common)

target_sources(stapikcommon PRIVATE
        src/stapik/app/AppContext.cpp
        src/stapik/app/BundledRuntime.cpp
        src/stapik/app/CommandLine.cpp
        src/stapik/app/SingleInstanceGuard.cpp
        src/stapik/app/StapikApplication.cpp
        src/stapik/app/UserAgent.cpp
)

target_sources(stapikcommon PRIVATE
        src/stapik/archive/AppArchive.cpp
        src/stapik/archive/ZipReader.cpp
        src/stapik/archive/ZipWriter.cpp
)

target_sources(stapikcommon PRIVATE
        src/stapik/cloud/ApiKeyExpiry.cpp
        src/stapik/cloud/AssetCache.cpp
        src/stapik/cloud/CloudAssetClient.cpp
        src/stapik/cloud/CloudAssetProtocol.cpp
        src/stapik/cloud/CloudChangesProtocol.cpp
        src/stapik/cloud/CloudPartitionProtocol.cpp
        src/stapik/cloud/CloudStorageClient.cpp
        src/stapik/cloud/CloudStorageFactory.cpp
        src/stapik/cloud/CompressedCloudStorage.cpp
        src/stapik/cloud/ConnectionCheck.cpp
        src/stapik/cloud/EncryptedCloudStorage.cpp
        src/stapik/cloud/RateLimit.cpp
        src/stapik/cloud/SizeWarningAssetStorage.cpp
        src/stapik/cloud/TransformingCloudStorage.cpp
)

target_sources(stapikcommon PRIVATE
        src/stapik/command/CompositeCommand.cpp
        src/stapik/command/UndoStack.cpp
)

target_sources(stapikcommon PRIVATE
        src/stapik/compression/Gzip.cpp
)

target_sources(stapikcommon PRIVATE
        src/stapik/crypto/PayloadCipher.cpp
)

target_sources(stapikcommon PRIVATE
        src/stapik/csv/CsvWriter.cpp
)

target_sources(stapikcommon PRIVATE
        src/stapik/document/DocumentFileCore.cpp
        src/stapik/document/SchemaMigrator.cpp
)

target_sources(stapikcommon PRIVATE
        src/stapik/domain/CategoryColor.cpp
        src/stapik/domain/CurrencyCatalog.cpp
        src/stapik/domain/DateRange.cpp
        src/stapik/domain/IdGenerator.cpp
        src/stapik/domain/Money.cpp
        src/stapik/domain/MoneyFormatter.cpp
        src/stapik/domain/YearMonth.cpp
        src/stapik/domain/YearMonthRange.cpp
        src/stapik/domain/YearRange.cpp
)

target_sources(stapikcommon PRIVATE
        src/stapik/http/CurlHttpClient.cpp
        src/stapik/http/CurlProxy.cpp
        src/stapik/http/CurlSecurity.cpp
        src/stapik/http/DefaultHttpClient.cpp
        src/stapik/http/HttpMessage.cpp
        src/stapik/http/ProxyList.cpp
        src/stapik/http/RetryAfter.cpp
        src/stapik/http/RetryingHttpClient.cpp
        src/stapik/http/SystemProxy.cpp
)

target_sources(stapikcommon PRIVATE
        src/stapik/locale/DateFormatter.cpp
        src/stapik/locale/LanguageCode.cpp
        src/stapik/locale/LanguageRegistry.cpp
        src/stapik/locale/LocaleManager.cpp
        src/stapik/locale/LocalizationEngine.cpp
        src/stapik/locale/PluralRules.cpp
        src/stapik/locale/RelativeTime.cpp
        src/stapik/locale/SystemLanguage.cpp
)

target_sources(stapikcommon PRIVATE
        src/stapik/log/ConsoleSink.cpp
        src/stapik/log/FileSink.cpp
        src/stapik/log/GLibLogBridge.cpp
        src/stapik/log/Log.cpp
        src/stapik/log/LogFile.cpp
        src/stapik/log/LogRecord.cpp
        src/stapik/log/LogReport.cpp
        src/stapik/log/MemoryLog.cpp
        src/stapik/log/RingBufferSink.cpp
)

target_sources(stapikcommon PRIVATE
        src/stapik/markdown/MarkdownParser.cpp
)

target_sources(stapikcommon PRIVATE
        src/stapik/settings/AppSettings.cpp
        src/stapik/settings/SettingsStore.cpp
)

target_sources(stapikcommon PRIVATE
        src/stapik/storage/AppPaths.cpp
        src/stapik/storage/AtomicFile.cpp
        src/stapik/storage/CloudStorageConfigStorage.cpp
        src/stapik/storage/DirectoryLock.cpp
        src/stapik/storage/SecretStore.cpp
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
        src/stapik/text/Base64.cpp
        src/stapik/text/Collation.cpp
        src/stapik/text/TextFolding.cpp
)

target_sources(stapikcommon PRIVATE
        src/stapik/theme/FontChoice.cpp
        src/stapik/theme/FontManager.cpp
        src/stapik/theme/FontScale.cpp
        src/stapik/theme/Theme.cpp
        src/stapik/theme/ThemeManager.cpp
        src/stapik/theme/ThemeRegistry.cpp
)

target_sources(stapikcommon PRIVATE
        src/stapik/time/Clock.cpp
)

target_sources(stapikcommon PRIVATE
        src/stapik/tutorial/Tutorial.cpp
        src/stapik/tutorial/TutorialLoader.cpp
)

target_sources(stapikcommon PRIVATE
        src/stapik/ui/dialog/AboutDialog.cpp
        src/stapik/ui/dialog/ChoiceDialog.cpp
        src/stapik/ui/dialog/ConfirmDialog.cpp
        src/stapik/ui/dialog/ConnectDialog.cpp
        src/stapik/ui/dialog/DialogUtils.cpp
        src/stapik/ui/dialog/FileDialogs.cpp
        src/stapik/ui/dialog/FileDrop.cpp
        src/stapik/ui/dialog/LastDirectory.cpp
        src/stapik/ui/dialog/LogViewerDialog.cpp
        src/stapik/ui/dialog/RestoreBackupDialog.cpp
        src/stapik/ui/dialog/SettingsDialog.cpp
        src/stapik/ui/dialog/StapikDialog.cpp
        src/stapik/ui/dialog/TextInputDialog.cpp
        src/stapik/ui/dialog/TutorialWindow.cpp
        src/stapik/ui/dialog/UpdateDialog.cpp
        src/stapik/ui/menu/RadioAction.cpp
        src/stapik/ui/menu/StandardMenu.cpp
        src/stapik/ui/menu/UndoActions.cpp
        src/stapik/ui/print/PdfExport.cpp
        src/stapik/ui/print/Printing.cpp
        src/stapik/ui/settings/BoolSettingRow.cpp
        src/stapik/ui/settings/ChoiceDropDown.cpp
        src/stapik/ui/settings/FontSettingRow.cpp
        src/stapik/ui/settings/PathSettingRow.cpp
        src/stapik/ui/settings/SettingsLayout.cpp
        src/stapik/ui/style/AppStyleProvider.cpp
        src/stapik/ui/widget/CalendarButton.cpp
        src/stapik/ui/widget/CategoryChip.cpp
        src/stapik/ui/widget/DatePicker.cpp
        src/stapik/ui/widget/EmptyState.cpp
        src/stapik/ui/widget/MarkdownView.cpp
        src/stapik/ui/widget/MoneyEntry.cpp
        src/stapik/ui/widget/MonthPicker.cpp
        src/stapik/ui/widget/SearchEntry.cpp
        src/stapik/ui/widget/SectionHeader.cpp
        src/stapik/ui/widget/StatusIndicator.cpp
        src/stapik/ui/window/PanedPosition.cpp
        src/stapik/ui/window/WindowPlacement.cpp
        src/stapik/ui/window/WindowState.cpp
)

target_sources(stapikcommon PRIVATE
        src/stapik/update/GitHubRelease.cpp
        src/stapik/update/GitHubReleaseClient.cpp
        src/stapik/update/Semver.cpp
        src/stapik/update/UpdateChecker.cpp
)

stapik_enable_warnings(stapikcommon)
stapik_enable_sanitizers(stapikcommon)
stapik_enable_coverage(stapikcommon)

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

target_link_libraries(stapikcommon PRIVATE ZLIB::ZLIB PkgConfig::SODIUM)

if(WIN32)
    # MessageBoxW and AllowSetForegroundWindow of SingleInstanceGuard, window placement
    target_link_libraries(stapikcommon PUBLIC user32)
    # Credential Manager of SecretStore
    target_link_libraries(stapikcommon PUBLIC advapi32)
    # System proxy settings of the HTTP client
    target_link_libraries(stapikcommon PUBLIC winhttp)
endif()

if(TARGET PkgConfig::X11)
    target_link_libraries(stapikcommon PUBLIC PkgConfig::X11)
    target_compile_definitions(stapikcommon PRIVATE STAPIK_HAS_X11)
endif()

if(TARGET PkgConfig::LIBSECRET)
    target_link_libraries(stapikcommon PUBLIC PkgConfig::LIBSECRET)
    target_compile_definitions(stapikcommon PRIVATE STAPIK_HAS_LIBSECRET)
endif()

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
            cmake/StapikQuality.cmake
            cmake/StapikWarnings.cmake
            DESTINATION "${STAPIK_COMMON_CMAKE_INSTALL_DIR}")
    install(DIRECTORY cmake/templates DESTINATION "${STAPIK_COMMON_CMAKE_INSTALL_DIR}")
endif()

if(STAPIK_COMMON_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
