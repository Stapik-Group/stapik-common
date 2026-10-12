include(FetchContent)

FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/releases/download/v1.17.0/googletest-1.17.0.tar.gz
        FIND_PACKAGE_ARGS NAMES GTest
)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

add_executable(stapikcommon_tests
        app/AppContextTest.cpp
        app/BundledRuntimeTest.cpp
        app/CommandLineTest.cpp
        app/InstanceMessageTest.cpp
        app/SingleInstanceGuardTest.cpp
        archive/AppArchiveTest.cpp
        archive/ZipTest.cpp
        cloud/AssetCacheTest.cpp
        cloud/CloudAssetProtocolTest.cpp
        cloud/CloudPartitionProtocolTest.cpp
        cloud/CloudPartitionsTest.cpp
        cloud/CloudStorageConfigTest.cpp
        command/UndoStackTest.cpp
        core/ResultTest.cpp
        crypto/PayloadCipherTest.cpp
        csv/CsvWriterTest.cpp
        document/DocumentFileTest.cpp
        domain/CurrencyCatalogTest.cpp
        domain/DateRangeTest.cpp
        domain/MoneyFormatterTest.cpp
        domain/MoneyTest.cpp
        domain/YearMonthRangeTest.cpp
        domain/YearRangeTest.cpp
        document/SchemaMigratorTest.cpp
        locale/DateFormatterTest.cpp
        locale/LanguageCodeTest.cpp
        locale/LanguageRegistryTest.cpp
        locale/LocalizationEngineTest.cpp
        locale/PluralRulesTest.cpp
        locale/RelativeTimeTest.cpp
        locale/SystemLanguageTest.cpp
        log/ConsoleSinkTest.cpp
        log/FileSinkTest.cpp
        log/GLibLogBridgeTest.cpp
        log/LogFileTest.cpp
        log/LogReportTest.cpp
        log/LogTest.cpp
        log/RingBufferSinkTest.cpp
        resources/CommonResourcesTest.cpp
        settings/AppSettingsTest.cpp
        settings/EnumSettingsTest.cpp
        settings/ObservableSettingTest.cpp
        settings/SettingsStoreTest.cpp
        storage/AppPathsTest.cpp
        storage/AtomicFileTest.cpp
        storage/CloudStorageConfigStorageTest.cpp
        storage/DirectoryLockTest.cpp
        storage/FileErrorTest.cpp
        storage/PathTextTest.cpp
        sync/AsyncSyncSchedulerPartitionTest.cpp
        sync/AsyncSyncSchedulerTest.cpp
        sync/CloudSessionPartitionTest.cpp
        sync/CloudSessionTest.cpp
        sync/PartitionKeyTest.cpp
        sync/PartitionedSyncServiceTest.cpp
        sync/SyncCoordinatorPartitionTest.cpp
        sync/SyncCoordinatorTest.cpp
        sync/SyncStatusTest.cpp
        sync/TimestampTest.cpp
        task/BackgroundTaskRunnerTest.cpp
        task/DebouncedActionTest.cpp
        text/CollationTest.cpp
        text/TextFoldingTest.cpp
        text/TrimTest.cpp
        theme/ThemeRegistryTest.cpp
        theme/ThemeTest.cpp
        time/ClockTest.cpp
        ui/PanedPositionTest.cpp
        ui/RadioOptionsTest.cpp
        ui/WindowStateTest.cpp
        update/GitHubReleaseTest.cpp
        update/SemverTest.cpp
        update/UpdateCheckerTest.cpp
)

target_link_libraries(stapikcommon_tests PRIVATE
        stapikcommon
        GTest::gtest_main
)

target_include_directories(stapikcommon_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(stapikcommon_tests PRIVATE
        STAPIK_COMMON_RESOURCES_DIR="${PROJECT_SOURCE_DIR}/resources"
)

target_compile_options(stapikcommon_tests PRIVATE -Wall -Wextra -Wpedantic)

gtest_discover_tests(stapikcommon_tests)
