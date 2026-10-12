#include "SizeWarningAssetStorage.hpp"

#include "stapik/log/Log.hpp"

#include <utility>

namespace stapik::cloud
{
    SizeWarningAssetStorage::SizeWarningAssetStorage(const IAssetStorage& inner, const AssetSizeLimit limit, WarningHandler onWarning) :
        m_inner(inner),
        m_limit(limit),
        m_onWarning(std::move(onWarning))
    {}

    std::vector<AssetInfo> SizeWarningAssetStorage::listAssets() const
    {
        return m_inner.listAssets();
    }

    std::optional<AssetBytes> SizeWarningAssetStorage::downloadAsset(const std::string& filename) const
    {
        return m_inner.downloadAsset(filename);
    }

    bool SizeWarningAssetStorage::deleteAsset(const std::string& filename) const
    {
        return m_inner.deleteAsset(filename);
    }

    AssetInfo SizeWarningAssetStorage::uploadAsset(const std::string& filename, const std::string& mimeType, const AssetBytes& content, const AssetUploadHooks& hooks) const
    {
        const auto sizeBytes = static_cast<std::int64_t>(content.size());

        if (const auto status = m_limit.assess(sizeBytes); status != AssetSizeStatus::Within)
        {
            log::warning("File '{}' ({} bytes) is {} the cloud slot limit of {} bytes", filename, sizeBytes,
                         status == AssetSizeStatus::OverLimit ? "over" : "close to", m_limit.maxBytes);

            if (m_onWarning)
                m_onWarning(AssetSizeWarning{ .filename = filename, .sizeBytes = sizeBytes, .limit = m_limit, .status = status });
        }

        return m_inner.uploadAsset(filename, mimeType, content, hooks);
    }
}
