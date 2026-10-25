#pragma once

#include <string>
#include <utility>

namespace stapik::storage
{
    enum class SecretLookupStatus
    {
        Found,
        Missing,
        Failed
    };

    struct SecretLookup
    {
        SecretLookupStatus status = SecretLookupStatus::Missing;
        std::string secret;

        [[nodiscard]] static SecretLookup found(std::string value)
        {
            return { SecretLookupStatus::Found, std::move(value) };
        }

        [[nodiscard]] static SecretLookup missing()
        {
            return { SecretLookupStatus::Missing, {} };
        }

        [[nodiscard]] static SecretLookup failed()
        {
            return { SecretLookupStatus::Failed, {} };
        }

        [[nodiscard]] bool isFound() const
        {
            return status == SecretLookupStatus::Found;
        }

        [[nodiscard]] bool isFailed() const
        {
            return status == SecretLookupStatus::Failed;
        }
    };

    class ISecretStore
    {
    public:
        virtual ~ISecretStore() = default;

        [[nodiscard]] virtual bool store(const std::string& service, const std::string& account, const std::string& secret) = 0;
        [[nodiscard]] virtual SecretLookup lookup(const std::string& service, const std::string& account) = 0;
        virtual bool remove(const std::string& service, const std::string& account) = 0;
    };

    [[nodiscard]] ISecretStore& platformSecretStore();
}
