#include "PayloadCipher.hpp"

#include <sodium.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include <map>
#include <mutex>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace stapik::crypto
{
    namespace
    {
        constexpr std::string_view FORMAT = "stapik-e2e";
        constexpr int VERSION = 1;
        constexpr std::size_t MAX_CACHED_KEYS = 8;
        constexpr int BASE64_VARIANT = sodium_base64_VARIANT_ORIGINAL;

        constexpr std::size_t KEY_BYTES = crypto_aead_xchacha20poly1305_ietf_KEYBYTES;
        constexpr std::size_t NONCE_BYTES = crypto_aead_xchacha20poly1305_ietf_NPUBBYTES;
        constexpr std::size_t TAG_BYTES = crypto_aead_xchacha20poly1305_ietf_ABYTES;
        constexpr std::size_t SALT_BYTES = crypto_pwhash_SALTBYTES;

        using Bytes = std::vector<unsigned char>;

        void ensureSodiumInitialized()
        {
            static std::once_flag once;
            std::call_once(once, []
            {
                if (sodium_init() < 0)
                    throw std::runtime_error("Cannot initialize the cryptographic library");
            });
        }

        class SecretKey
        {
        public:
            SecretKey() = default;
            SecretKey(const SecretKey&) = default;
            SecretKey& operator=(const SecretKey&) = default;

            ~SecretKey()
            {
                sodium_memzero(m_bytes.data(), m_bytes.size());
            }

            [[nodiscard]] unsigned char* data()
            {
                return m_bytes.data();
            }

            [[nodiscard]] const unsigned char* data() const
            {
                return m_bytes.data();
            }

        private:
            std::array<unsigned char, KEY_BYTES> m_bytes{};
        };

        std::string toBase64(const std::span<const unsigned char> bytes)
        {
            std::string text(sodium_base64_ENCODED_LEN(bytes.size(), BASE64_VARIANT), '\0');
            sodium_bin2base64(text.data(), text.size(), bytes.data(), bytes.size(), BASE64_VARIANT);
            text.pop_back();
            return text;
        }

        std::optional<Bytes> fromBase64(const std::string& text)
        {
            Bytes bytes(text.size());
            std::size_t length = 0;
            if (sodium_base642bin(bytes.data(), bytes.size(), text.data(), text.size(), nullptr, &length, nullptr, BASE64_VARIANT) != 0)
                return std::nullopt;

            bytes.resize(length);
            return bytes;
        }

        std::optional<Bytes> bytesField(const nlohmann::json& blob, const char* name)
        {
            const auto found = blob.find(name);
            if (found == blob.end() || !found->is_string())
                return std::nullopt;

            return fromBase64(found->get<std::string>());
        }

        std::string associatedData(const std::string_view context)
        {
            return std::format("{}/{}/{}", FORMAT, VERSION, context);
        }

        const unsigned char* asBytes(const std::string_view text)
        {
            return reinterpret_cast<const unsigned char*>(text.data());
        }

        unsigned char* asBytes(std::string& text)
        {
            return reinterpret_cast<unsigned char*>(text.data());
        }
    }

    std::string_view describe(const DecryptionError error)
    {
        switch (error)
        {
            using enum DecryptionError;
            case NotEncrypted: return "the data is not encrypted";
            case UnsupportedVersion: return "the data was encrypted by a newer version of the application";
            case Malformed: return "the encrypted data is damaged";
            case AuthenticationFailed: return "the passphrase is wrong or the data was modified";
        }

        return "unknown error";
    }

    struct PayloadCipher::Impl
    {
        explicit Impl(std::string passphraseText) :
            passphrase(std::move(passphraseText))
        {
            ensureSodiumInitialized();
            randombytes_buf(writeSalt.data(), writeSalt.size());
        }

        ~Impl()
        {
            sodium_memzero(passphrase.data(), passphrase.size());
        }

        Impl(const Impl&) = delete;
        Impl& operator=(const Impl&) = delete;

        [[nodiscard]] SecretKey keyFor(const std::span<const unsigned char> salt)
        {
            const std::string id(reinterpret_cast<const char*>(salt.data()), salt.size());

            const std::lock_guard lock(mutex);
            if (const auto cached = keys.find(id); cached != keys.end())
                return cached->second;

            if (keys.size() >= MAX_CACHED_KEYS)
                dropForeignKeys();

            return keys.emplace(id, derive(salt)).first->second;
        }

        [[nodiscard]] nlohmann::json encrypt(const std::string_view plaintext, const std::string_view context)
        {
            std::array<unsigned char, NONCE_BYTES> nonce{};
            randombytes_buf(nonce.data(), nonce.size());

            const auto key = keyFor(writeSalt);
            const auto aad = associatedData(context);

            std::string ciphertext(plaintext.size() + TAG_BYTES, '\0');
            unsigned long long ciphertextLength = 0;
            crypto_aead_xchacha20poly1305_ietf_encrypt(
                asBytes(ciphertext), &ciphertextLength,
                asBytes(plaintext), plaintext.size(),
                asBytes(aad), aad.size(),
                nullptr, nonce.data(), key.data());
            ciphertext.resize(ciphertextLength);

            return {
                { "format", std::string(FORMAT) },
                { "version", VERSION },
                { "salt", toBase64(writeSalt) },
                { "nonce", toBase64(nonce) },
                { "ciphertext", toBase64(std::span(asBytes(ciphertext), ciphertext.size())) }
            };
        }

        [[nodiscard]] Result<std::string, DecryptionError> decrypt(const nlohmann::json& blob, const std::string_view context)
        {
            if (!isEncrypted(blob))
                return Failure(DecryptionError::NotEncrypted);

            const auto version = blob.find("version");
            if (version == blob.end() || !version->is_number_integer())
                return Failure(DecryptionError::Malformed);

            if (version->get<int>() != VERSION)
                return Failure(DecryptionError::UnsupportedVersion);

            const auto salt = bytesField(blob, "salt");
            const auto nonce = bytesField(blob, "nonce");
            const auto ciphertext = bytesField(blob, "ciphertext");
            if (!salt || !nonce || !ciphertext
                || salt->size() != SALT_BYTES || nonce->size() != NONCE_BYTES || ciphertext->size() < TAG_BYTES)
                return Failure(DecryptionError::Malformed);

            const auto key = keyFor(*salt);
            const auto aad = associatedData(context);

            std::string plaintext(ciphertext->size() - TAG_BYTES, '\0');
            unsigned long long plaintextLength = 0;
            if (crypto_aead_xchacha20poly1305_ietf_decrypt(
                    asBytes(plaintext), &plaintextLength,
                    nullptr,
                    ciphertext->data(), ciphertext->size(),
                    asBytes(aad), aad.size(),
                    nonce->data(), key.data()) != 0)
                return Failure(DecryptionError::AuthenticationFailed);

            plaintext.resize(plaintextLength);
            return plaintext;
        }

        std::string passphrase;
        std::array<unsigned char, SALT_BYTES> writeSalt{};
        std::mutex mutex;
        std::map<std::string, SecretKey> keys;

    private:
        [[nodiscard]] SecretKey derive(const std::span<const unsigned char> salt) const
        {
            SecretKey key;
            if (crypto_pwhash(
                    key.data(), KEY_BYTES,
                    passphrase.data(), passphrase.size(),
                    salt.data(),
                    crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE,
                    crypto_pwhash_ALG_ARGON2ID13) != 0)
                throw std::runtime_error("Not enough memory to derive the encryption key");

            return key;
        }

        void dropForeignKeys()
        {
            const std::string ownId(reinterpret_cast<const char*>(writeSalt.data()), writeSalt.size());
            std::erase_if(keys, [&ownId](const auto& item) { return item.first != ownId; });
        }
    };

    PayloadCipher::PayloadCipher(std::string passphrase) :
        m_impl(std::make_unique<Impl>(std::move(passphrase)))
    {}

    PayloadCipher::~PayloadCipher() = default;

    nlohmann::json PayloadCipher::encrypt(const std::string_view plaintext, const std::string_view context) const
    {
        return m_impl->encrypt(plaintext, context);
    }

    Result<std::string, DecryptionError> PayloadCipher::decrypt(const nlohmann::json& blob, const std::string_view context) const
    {
        return m_impl->decrypt(blob, context);
    }

    bool PayloadCipher::isEncrypted(const nlohmann::json& content)
    {
        if (!content.is_object())
            return false;

        const auto format = content.find("format");
        return format != content.end() && format->is_string() && format->get<std::string>() == FORMAT;
    }
}
