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
        cloud/CloudPartitionProtocolTest.cpp
        cloud/CloudPartitionsTest.cpp
        cloud/CloudStorageConfigTest.cpp
        command/UndoStackTest.cpp
        document/DocumentFileTest.cpp
        domain/CurrencyCatalogTest.cpp
        domain/MoneyFormatterTest.cpp
        domain/MoneyTest.cpp
        document/SchemaMigratorTest.cpp
        locale/LanguageRegistryTest.cpp
        locale/LocalizationEngineTest.cpp
        locale/SystemLanguageTest.cpp
        log/LogTest.cpp
        resources/CommonResourcesTest.cpp
        settings/AppSettingsTest.cpp
        settings/EnumSettingsTest.cpp
        settings/ObservableSettingTest.cpp
        settings/SettingsStoreTest.cpp
        storage/AppPathsTest.cpp
        storage/AtomicFileTest.cpp
        sync/AsyncSyncSchedulerTest.cpp
        sync/CloudSessionTest.cpp
        sync/PartitionKeyTest.cpp
        sync/PartitionedSyncServiceTest.cpp
        sync/SyncCoordinatorPartitionTest.cpp
        sync/SyncCoordinatorTest.cpp
        sync/SyncStatusTest.cpp
        sync/TimestampTest.cpp
        task/BackgroundTaskRunnerTest.cpp
        task/DebouncedActionTest.cpp
        text/TrimTest.cpp
        theme/ThemeRegistryTest.cpp
        theme/ThemeTest.cpp
        ui/RadioOptionsTest.cpp
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
