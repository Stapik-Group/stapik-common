#pragma once

#include "stapik/core/Result.hpp"

#include <nlohmann/json.hpp>

#include <memory>
#include <string>
#include <string_view>

namespace stapik::crypto
{
    enum class DecryptionError
    {
        NotEncrypted,
        UnsupportedVersion,
        Malformed,
        AuthenticationFailed
    };

    [[nodiscard]] std::string_view describe(DecryptionError error);

    class PayloadCipher
    {
    public:
        explicit PayloadCipher(std::string passphrase);
        ~PayloadCipher();

        PayloadCipher(const PayloadCipher&) = delete;
        PayloadCipher& operator=(const PayloadCipher&) = delete;

        [[nodiscard]] nlohmann::json encrypt(std::string_view plaintext, std::string_view context) const;
        [[nodiscard]] Result<std::string, DecryptionError> decrypt(const nlohmann::json& blob, std::string_view context) const;

        [[nodiscard]] static bool isEncrypted(const nlohmann::json& content);

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}
