#pragma once

#include <optional>
#include <string>

namespace stapik::storage
{
    class ISecretStore
    {
    public:
        virtual ~ISecretStore() = default;

        [[nodiscard]] virtual bool store(const std::string& service, const std::string& account, const std::string& secret) = 0;
        [[nodiscard]] virtual std::optional<std::string> lookup(const std::string& service, const std::string& account) = 0;
        virtual bool remove(const std::string& service, const std::string& account) = 0;
    };

    [[nodiscard]] ISecretStore& platformSecretStore();
}
