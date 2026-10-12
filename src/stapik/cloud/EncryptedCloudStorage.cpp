#include "EncryptedCloudStorage.hpp"

#include "stapik/log/Log.hpp"

#include <format>
#include <utility>

namespace stapik::cloud
{
    EncryptedCloudStorage::EncryptedCloudStorage(std::unique_ptr<ICloudStorage> inner, std::string passphrase, std::string scope) :
        TransformingCloudStorage(std::move(inner)),
        m_cipher(std::move(passphrase)),
        m_scope(std::move(scope))
    {}

    std::string EncryptedCloudStorage::scoped(const std::string& context) const
    {
        return m_scope + "/" + context;
    }

    nlohmann::json EncryptedCloudStorage::seal(const nlohmann::json& data, const std::string& context) const
    {
        return m_cipher.encrypt(data.dump(), scoped(context));
    }

    CloudDocument EncryptedCloudStorage::open(CloudDocument document, const std::string& context) const
    {
        if (document.content.is_object() && document.content.empty())
            return document;

        if (!crypto::PayloadCipher::isEncrypted(document.content))
        {
            log::warning("The cloud data of {} is not encrypted; it was saved before end-to-end encryption was turned on", scoped(context));
            return document;
        }

        const auto plaintext = m_cipher.decrypt(document.content, scoped(context));
        if (!plaintext)
            throw CloudDecryptionException(std::format("Cannot decrypt the cloud data: {}", crypto::describe(plaintext.error())));

        try
        {
            document.content = nlohmann::json::parse(plaintext.value());
        }
        catch (const nlohmann::json::exception&)
        {
            throw CloudDecryptionException("Cannot read the decrypted cloud data");
        }

        return document;
    }
}
