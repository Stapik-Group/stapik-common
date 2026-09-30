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
        cloud/CloudStorageConfigTest.cpp
        log/LogTest.cpp
        settings/AppSettingsTest.cpp
        settings/EnumSettingsTest.cpp
        settings/ObservableSettingTest.cpp
        settings/SettingsStoreTest.cpp
        storage/AppPathsTest.cpp
        storage/AtomicFileTest.cpp
        sync/TimestampTest.cpp
)

target_link_libraries(stapikcommon_tests PRIVATE
        stapikcommon
        GTest::gtest_main
)

target_include_directories(stapikcommon_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_options(stapikcommon_tests PRIVATE -Wall -Wextra -Wpedantic)

gtest_discover_tests(stapikcommon_tests)
