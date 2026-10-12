#pragma once

#include "TransformingCloudStorage.hpp"

#include "stapik/crypto/PayloadCipher.hpp"

#include <memory>
#include <string>

namespace stapik::cloud
{
    class EncryptedCloudStorage final : public TransformingCloudStorage
    {
    public:
        EncryptedCloudStorage(std::unique_ptr<ICloudStorage> inner, std::string passphrase, std::string scope);

    private:
        crypto::PayloadCipher m_cipher;
        std::string m_scope;

        [[nodiscard]] std::string scoped(const std::string& context) const;
        [[nodiscard]] nlohmann::json seal(const nlohmann::json& data, const std::string& context) const override;
        [[nodiscard]] CloudDocument open(CloudDocument document, const std::string& context) const override;
    };
}
