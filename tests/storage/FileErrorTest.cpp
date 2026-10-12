#include "stapik/storage/FileError.hpp"

#include <gtest/gtest.h>

#include <string>
#include <system_error>

namespace
{
    using stapik::storage::FileError;
}

TEST(FileErrorTest, MessageNamesTheStepThePathAndTheReason)
{
    const FileError error{
        .path = "data/settings.json",
        .step = "rename",
        .code = std::make_error_code(std::errc::permission_denied)
    };

    const auto message = error.message();

    EXPECT_NE(message.find("rename"), std::string::npos);
    EXPECT_NE(message.find("settings.json"), std::string::npos);
    EXPECT_NE(message.find(error.code.message()), std::string::npos);
}

TEST(FileErrorTest, CodeCanBeComparedWithPortableConditions)
{
    const FileError error{
        .path = "data/settings.json",
        .step = "write",
        .code = std::make_error_code(std::errc::no_space_on_device)
    };

    EXPECT_EQ(error.code, std::errc::no_space_on_device);
}
