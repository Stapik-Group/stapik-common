#pragma once

#include "stapik/storage/SecretStore.hpp"

#include <map>
#include <string>
#include <utility>

namespace stapik::test
{
    class FakeSecretStore final : public storage::ISecretStore
    {
    public:
        bool available = true;

        bool store(const std::string& service, const std::string& account, const std::string& secret) override
        {
            if (!available)
                return false;

            m_secrets[{ service, account }] = secret;
            return true;
        }

        std::optional<std::string> lookup(const std::string& service, const std::string& account) override
        {
            if (!available)
                return std::nullopt;

            const auto found = m_secrets.find({ service, account });
            if (found == m_secrets.end())
                return std::nullopt;

            return found->second;
        }

        bool remove(const std::string& service, const std::string& account) override
        {
            return available && m_secrets.erase({ service, account }) > 0;
        }

        [[nodiscard]] bool contains(const std::string& service, const std::string& account) const
        {
            return m_secrets.contains({ service, account });
        }

    private:
        std::map<std::pair<std::string, std::string>, std::string> m_secrets;
    };
}
