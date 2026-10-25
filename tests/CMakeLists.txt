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
        app/UserAgentTest.cpp
        archive/AppArchiveTest.cpp
        archive/ZipTest.cpp
        cloud/ApiKeyExpiryTest.cpp
        cloud/AssetCacheTest.cpp
        cloud/CloudAssetClientTest.cpp
        cloud/CloudAssetProtocolTest.cpp
        cloud/CloudChangesProtocolTest.cpp
        cloud/CloudPartitionProtocolTest.cpp
        cloud/CloudPartitionsTest.cpp
        cloud/CloudStorageClientTest.cpp
        cloud/CloudStorageConfigTest.cpp
        cloud/CloudStorageFactoryTest.cpp
        cloud/CompressedCloudStorageTest.cpp
        cloud/ConnectionCheckTest.cpp
        cloud/EncryptedCloudStorageTest.cpp
        cloud/RateLimitTest.cpp
        cloud/SizeWarningAssetStorageTest.cpp
        command/UndoStackTest.cpp
        compression/GzipTest.cpp
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
        http/CurlHttpClientTest.cpp
        http/HttpMessageTest.cpp
        http/ProxyListTest.cpp
        http/RetryAfterTest.cpp
        http/RetryingHttpClientTest.cpp
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
        markdown/MarkdownParserTest.cpp
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
        storage/QuarantineTest.cpp
        sync/AsyncSyncSchedulerPartitionTest.cpp
        sync/AsyncSyncSchedulerTest.cpp
        sync/CloudSessionPartitionTest.cpp
        sync/CloudSessionTest.cpp
        sync/PartitionKeyTest.cpp
        sync/PartitionedSyncServiceTest.cpp
        sync/SyncCoordinatorPartitionTest.cpp
        sync/SyncCoordinatorTest.cpp
        sync/SyncEnvelopeTest.cpp
        sync/SyncStatusTest.cpp
        sync/TimestampTest.cpp
        task/BackgroundTaskRunnerTest.cpp
        task/CancellationTest.cpp
        task/DebouncedActionTest.cpp
        task/ThreadPoolTest.cpp
        text/Base64Test.cpp
        text/CollationTest.cpp
        text/JsonTextTest.cpp
        text/TextFoldingTest.cpp
        text/TrimTest.cpp
        theme/FontChoiceTest.cpp
        theme/FontScaleTest.cpp
        theme/ThemeRegistryTest.cpp
        theme/ThemeTest.cpp
        time/ClockTest.cpp
        tutorial/TutorialLoaderTest.cpp
        tutorial/TutorialTest.cpp
        ui/FileDropTest.cpp
        ui/LastDirectoryTest.cpp
        ui/PanedPositionTest.cpp
        ui/PdfExportTest.cpp
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
stapik_enable_sanitizers(stapikcommon_tests)
stapik_enable_coverage(stapikcommon_tests)

gtest_discover_tests(stapikcommon_tests)
