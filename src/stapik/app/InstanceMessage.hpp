#pragma once

#include "stapik/storage/PathText.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace stapik::app
{
    // What a second instance tells the running one: the files it was asked to open. UTF-8 paths, one per line
    // (a Windows file name cannot contain a newline), after a header that lets the receiver reject stray data.
    inline constexpr std::string_view INSTANCE_MESSAGE_HEADER = "stapik-instance/1\n";

    [[nodiscard]] inline std::string encodeInstanceMessage(const std::vector<std::filesystem::path>& files)
    {
        std::string message(INSTANCE_MESSAGE_HEADER);
        for (const auto& file : files)
        {
            message += storage::pathText(file);
            message += '\n';
        }
        return message;
    }

    [[nodiscard]] inline std::optional<std::vector<std::filesystem::path>> decodeInstanceMessage(std::string_view message)
    {
        if (!message.starts_with(INSTANCE_MESSAGE_HEADER))
            return std::nullopt;

        message.remove_prefix(INSTANCE_MESSAGE_HEADER.size());

        std::vector<std::filesystem::path> files;
        while (!message.empty())
        {
            const auto end = message.find('\n');
            if (const auto line = message.substr(0, end); !line.empty())
                files.push_back(storage::pathFromText(std::string(line)));

            if (end == std::string_view::npos)
                break;

            message.remove_prefix(end + 1);
        }
        return files;
    }
}
