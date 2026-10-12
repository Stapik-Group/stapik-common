#pragma once

#include "stapik/cloud/CloudAssetProtocol.hpp"
#include "stapik/cloud/IAssetStorage.hpp"

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace stapik::test
{
    class FakeAssetStorage final : public cloud::IAssetStorage
    {
    public:
        struct File
        {
            std::string mimeType;
            cloud::AssetBytes content;
        };

        mutable std::map<std::string, File> files;
        mutable std::vector<std::string> uploaded;

        bool unreachable = false;
        // Listed, but gone when downloaded.
        std::set<std::string> vanishing;
        std::set<std::string> tooLarge;

        void put(const std::string& filename, const std::string& text, const std::string& mimeType = "text/plain") const
        {
            files[filename] = File{ mimeType, cloud::AssetBytes(text.begin(), text.end()) };
        }

        [[nodiscard]] std::string text(const std::string& filename) const
        {
            const auto& content = files.at(filename).content;
            return { content.begin(), content.end() };
        }

        [[nodiscard]] std::vector<cloud::AssetInfo> listAssets() const override
        {
            failIfUnreachable();

            std::vector<cloud::AssetInfo> result;
            for (const auto& [filename, file] : files)
                result.push_back(describe(filename, file));
            return result;
        }

        [[nodiscard]] std::optional<cloud::AssetBytes> downloadAsset(const std::string& filename) const override
        {
            failIfUnreachable();

            if (vanishing.contains(filename))
                return std::nullopt;

            const auto found = files.find(filename);
            if (found == files.end())
                return std::nullopt;

            return found->second.content;
        }

        cloud::AssetInfo uploadAsset(const std::string& filename, const std::string& mimeType, const cloud::AssetBytes& content) const override
        {
            failIfUnreachable();

            if (tooLarge.contains(filename))
                throw cloud::AssetTooLargeException("too large");

            files[filename] = File{ mimeType, content };
            uploaded.push_back(filename);
            return describe(filename, files[filename]);
        }

        [[nodiscard]] bool deleteAsset(const std::string& filename) const override
        {
            failIfUnreachable();
            return files.erase(filename) > 0;
        }

    private:
        void failIfUnreachable() const
        {
            if (unreachable)
                throw ::CloudStorageException("network unreachable");
        }

        static cloud::AssetInfo describe(const std::string& filename, const File& file)
        {
            return cloud::AssetInfo{
                .filename = filename,
                .mimeType = file.mimeType,
                .sizeBytes = static_cast<std::int64_t>(file.content.size()),
                .checksumSha256 = cloud::sha256Hex(file.content),
                .updatedAt = {} };
        }
    };
}
