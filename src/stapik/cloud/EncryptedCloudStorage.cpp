#include "EncryptedCloudStorage.hpp"

#include "stapik/log/Log.hpp"

#include <format>
#include <utility>

namespace stapik::cloud
{
    EncryptedCloudStorage::EncryptedCloudStorage(std::unique_ptr<ICloudStorage> inner, std::string passphrase, std::string scope) :
        m_inner(std::move(inner)),
        m_cipher(std::move(passphrase)),
        m_scope(std::move(scope))
    {}

    std::string EncryptedCloudStorage::documentContext() const
    {
        return m_scope + "/document";
    }

    std::string EncryptedCloudStorage::partitionContext(const std::string& partition) const
    {
        return m_scope + "/partition/" + partition;
    }

    nlohmann::json EncryptedCloudStorage::seal(const nlohmann::json& data, const std::string& context) const
    {
        return m_cipher.encrypt(data.dump(), context);
    }

    CloudDocument EncryptedCloudStorage::open(CloudDocument document, const std::string& context) const
    {
        if (document.content.is_object() && document.content.empty())
            return document;

        if (!crypto::PayloadCipher::isEncrypted(document.content))
        {
            log::warning("The cloud data of {} is not encrypted; it was saved before end-to-end encryption was turned on", context);
            return document;
        }

        const auto plaintext = m_cipher.decrypt(document.content, context);
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

    std::optional<CloudDocument> EncryptedCloudStorage::openIfPresent(std::optional<CloudDocument> document, const std::string& context) const
    {
        if (!document)
            return std::nullopt;

        return open(std::move(*document), context);
    }

    CloudWriteResult EncryptedCloudStorage::write(CloudWriteResult result, const nlohmann::json& sentData, const std::string& context) const
    {
        if (result.conflict)
            return { .document = open(std::move(result.document), context), .conflict = true };

        result.document.content = sentData;
        return result;
    }

    std::optional<CloudDocument> EncryptedCloudStorage::loadDocument() const
    {
        return openIfPresent(m_inner->loadDocument(), documentContext());
    }

    CloudWriteResult EncryptedCloudStorage::saveDocument(const nlohmann::json& data, const std::chrono::system_clock::time_point clientLastKnownUpdate) const
    {
        const auto context = documentContext();
        return write(m_inner->saveDocument(seal(data, context), clientLastKnownUpdate), data, context);
    }

    std::vector<CloudPartitionInfo> EncryptedCloudStorage::listPartitions() const
    {
        return m_inner->listPartitions();
    }

    std::optional<CloudDocument> EncryptedCloudStorage::loadPartition(const std::string& partition) const
    {
        return openIfPresent(m_inner->loadPartition(partition), partitionContext(partition));
    }

    CloudWriteResult EncryptedCloudStorage::savePartition(const std::string& partition, const nlohmann::json& data, const std::chrono::system_clock::time_point clientLastKnownUpdate) const
    {
        const auto context = partitionContext(partition);
        return write(m_inner->savePartition(partition, seal(data, context), clientLastKnownUpdate), data, context);
    }

    bool EncryptedCloudStorage::deletePartition(const std::string& partition) const
    {
        return m_inner->deletePartition(partition);
    }
}
