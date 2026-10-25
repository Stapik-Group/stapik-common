#include "stapik/crypto/PayloadCipher.hpp"

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include <cstddef>
#include <string>
#include <utility>

using stapik::crypto::DecryptionError;
using stapik::crypto::PayloadCipher;
using stapik::crypto::PayloadEncoding;

namespace
{
    constexpr auto PASSPHRASE = "correct horse battery staple";
    constexpr auto CONTEXT = "slot/document";

    nlohmann::json objectWith(const std::string& key, nlohmann::json value)
    {
        auto object = nlohmann::json::object();
        object[key] = std::move(value);
        return object;
    }

    void flipCharacter(nlohmann::json& blob, const char* field, const std::size_t index)
    {
        auto text = blob.at(field).get<std::string>();
        text[index] = text[index] == 'A' ? 'B' : 'A';
        blob[field] = text;
    }
}

TEST(PayloadCipherTest, DecryptReturnsWhatWasEncrypted)
{
    const PayloadCipher cipher(PASSPHRASE);

    const auto blob = cipher.encrypt("{\"amount\":12}", CONTEXT);
    const auto plaintext = cipher.decrypt(blob, CONTEXT);

    ASSERT_TRUE(plaintext.hasValue());
    EXPECT_EQ(plaintext.value(), "{\"amount\":12}");
}

TEST(PayloadCipherTest, EmptyAndNonAsciiTextsRoundTrip)
{
    const PayloadCipher cipher(PASSPHRASE);

    for (const std::string text : { std::string{}, std::string("zażółć gęślą jaźń"), std::string("a\0b", 3) })
    {
        const auto plaintext = cipher.decrypt(cipher.encrypt(text, CONTEXT), CONTEXT);

        ASSERT_TRUE(plaintext.hasValue());
        EXPECT_EQ(plaintext.value(), text);
    }
}

TEST(PayloadCipherTest, BlobDoesNotContainPlaintext)
{
    const PayloadCipher cipher(PASSPHRASE);

    const auto blob = cipher.encrypt("very recognizable secret text", CONTEXT);

    EXPECT_EQ(blob.dump().find("recognizable"), std::string::npos);
}

TEST(PayloadCipherTest, EncryptingTwiceGivesDifferentBlobs)
{
    const PayloadCipher cipher(PASSPHRASE);

    EXPECT_NE(cipher.encrypt("text", CONTEXT), cipher.encrypt("text", CONTEXT));
}

TEST(PayloadCipherTest, AnotherInstanceWithTheSamePassphraseDecrypts)
{
    const PayloadCipher writer(PASSPHRASE);
    const PayloadCipher reader(PASSPHRASE);

    const auto plaintext = reader.decrypt(writer.encrypt("text", CONTEXT), CONTEXT);

    ASSERT_TRUE(plaintext.hasValue());
    EXPECT_EQ(plaintext.value(), "text");
}

TEST(PayloadCipherTest, WrongPassphraseFailsAuthentication)
{
    const auto blob = PayloadCipher(PASSPHRASE).encrypt("text", CONTEXT);

    const auto plaintext = PayloadCipher("other passphrase").decrypt(blob, CONTEXT);

    ASSERT_FALSE(plaintext.hasValue());
    EXPECT_EQ(plaintext.error(), DecryptionError::AuthenticationFailed);
}

TEST(PayloadCipherTest, WrongContextFailsAuthentication)
{
    const PayloadCipher cipher(PASSPHRASE);
    const auto blob = cipher.encrypt("text", "slot/partition/2025");

    const auto plaintext = cipher.decrypt(blob, "slot/partition/2024");

    ASSERT_FALSE(plaintext.hasValue());
    EXPECT_EQ(plaintext.error(), DecryptionError::AuthenticationFailed);
}

TEST(PayloadCipherTest, ModifiedCiphertextFailsAuthentication)
{
    const PayloadCipher cipher(PASSPHRASE);
    auto blob = cipher.encrypt("text to protect", CONTEXT);
    flipCharacter(blob, "ciphertext", 2);

    const auto plaintext = cipher.decrypt(blob, CONTEXT);

    ASSERT_FALSE(plaintext.hasValue());
    EXPECT_EQ(plaintext.error(), DecryptionError::AuthenticationFailed);
}

TEST(PayloadCipherTest, ModifiedNonceFailsAuthentication)
{
    const PayloadCipher cipher(PASSPHRASE);
    auto blob = cipher.encrypt("text to protect", CONTEXT);
    flipCharacter(blob, "nonce", 2);

    const auto plaintext = cipher.decrypt(blob, CONTEXT);

    ASSERT_FALSE(plaintext.hasValue());
    EXPECT_EQ(plaintext.error(), DecryptionError::AuthenticationFailed);
}

TEST(PayloadCipherTest, ModifiedSaltFailsAuthentication)
{
    const PayloadCipher cipher(PASSPHRASE);
    auto blob = cipher.encrypt("text to protect", CONTEXT);
    flipCharacter(blob, "salt", 2);

    const auto plaintext = cipher.decrypt(blob, CONTEXT);

    ASSERT_FALSE(plaintext.hasValue());
    EXPECT_EQ(plaintext.error(), DecryptionError::AuthenticationFailed);
}

TEST(PayloadCipherTest, NewerVersionIsReportedAsUnsupported)
{
    const PayloadCipher cipher(PASSPHRASE);
    auto blob = cipher.encrypt("text", CONTEXT);
    blob["version"] = 99;

    const auto plaintext = cipher.decrypt(blob, CONTEXT);

    ASSERT_FALSE(plaintext.hasValue());
    EXPECT_EQ(plaintext.error(), DecryptionError::UnsupportedVersion);
}

TEST(PayloadCipherTest, DamagedFieldsAreMalformed)
{
    const PayloadCipher cipher(PASSPHRASE);
    const auto blob = cipher.encrypt("text", CONTEXT);

    auto invalidBase64 = blob;
    invalidBase64["nonce"] = "!!!";

    auto missingField = blob;
    missingField.erase("ciphertext");

    auto wrongType = blob;
    wrongType["salt"] = 5;

    auto truncatedCiphertext = blob;
    truncatedCiphertext["ciphertext"] = "AAAA";

    auto missingVersion = blob;
    missingVersion.erase("version");

    for (const auto& damaged : { invalidBase64, missingField, wrongType, truncatedCiphertext, missingVersion })
    {
        const auto plaintext = cipher.decrypt(damaged, CONTEXT);

        ASSERT_FALSE(plaintext.hasValue());
        EXPECT_EQ(plaintext.error(), DecryptionError::Malformed);
    }
}

TEST(PayloadCipherTest, PlainJsonIsNotEncrypted)
{
    const PayloadCipher cipher(PASSPHRASE);

    const auto plaintext = cipher.decrypt(objectWith("lastUpdate", "2026-10-12T10:00:00Z"), CONTEXT);

    ASSERT_FALSE(plaintext.hasValue());
    EXPECT_EQ(plaintext.error(), DecryptionError::NotEncrypted);
}

TEST(PayloadCipherTest, IsEncryptedRecognizesOnlyBlobs)
{
    const PayloadCipher cipher(PASSPHRASE);

    EXPECT_TRUE(PayloadCipher::isEncrypted(cipher.encrypt("text", CONTEXT)));
    EXPECT_FALSE(PayloadCipher::isEncrypted(nlohmann::json::object()));
    EXPECT_FALSE(PayloadCipher::isEncrypted(nlohmann::json::array()));
    EXPECT_FALSE(PayloadCipher::isEncrypted(nlohmann::json(nullptr)));
    EXPECT_FALSE(PayloadCipher::isEncrypted(objectWith("format", 1)));
    EXPECT_FALSE(PayloadCipher::isEncrypted(objectWith("format", "something-else")));
}

TEST(PayloadCipherTest, EveryDecryptionErrorHasDescription)
{
    for (const auto error : { DecryptionError::NotEncrypted, DecryptionError::UnsupportedVersion, DecryptionError::Malformed, DecryptionError::AuthenticationFailed })
        EXPECT_FALSE(stapik::crypto::describe(error).empty());
}

TEST(PayloadCipherTest, GzipStreamPayloadRoundTripsBinaryBytes)
{
    const PayloadCipher cipher(PASSPHRASE);
    const std::string stream("\x1f\x8b\x08\x00\xff\xfe\x00binary", 14);

    const auto blob = cipher.encrypt(stream, CONTEXT, PayloadEncoding::GzipStream);
    const auto payload = cipher.decryptPayload(blob, CONTEXT);

    ASSERT_TRUE(payload.hasValue());
    EXPECT_EQ(payload.value().bytes, stream);
    EXPECT_EQ(payload.value().encoding, PayloadEncoding::GzipStream);
}

TEST(PayloadCipherTest, TextPayloadKeepsTheFirstVersionAndGzipStreamUsesTheSecond)
{
    const PayloadCipher cipher(PASSPHRASE);

    EXPECT_EQ(cipher.encrypt("text", CONTEXT).at("version"), 1);
    EXPECT_EQ(cipher.encrypt("text", CONTEXT, PayloadEncoding::GzipStream).at("version"), 2);
}

TEST(PayloadCipherTest, DecryptPayloadReportsTheEncodingOfTextToo)
{
    const PayloadCipher cipher(PASSPHRASE);

    const auto payload = cipher.decryptPayload(cipher.encrypt("text", CONTEXT), CONTEXT);

    ASSERT_TRUE(payload.hasValue());
    EXPECT_EQ(payload.value().encoding, PayloadEncoding::Text);
}

TEST(PayloadCipherTest, PlainDecryptRefusesAGzipStreamPayload)
{
    const PayloadCipher cipher(PASSPHRASE);
    const auto blob = cipher.encrypt("stream", CONTEXT, PayloadEncoding::GzipStream);

    const auto plaintext = cipher.decrypt(blob, CONTEXT);

    ASSERT_FALSE(plaintext.hasValue());
    EXPECT_EQ(plaintext.error(), DecryptionError::UnsupportedVersion);
}

TEST(PayloadCipherTest, EncodingCannotBeSwitchedByChangingTheVersion)
{
    const PayloadCipher cipher(PASSPHRASE);
    auto blob = cipher.encrypt("stream", CONTEXT, PayloadEncoding::GzipStream);
    blob["version"] = 1;

    const auto payload = cipher.decryptPayload(blob, CONTEXT);

    ASSERT_FALSE(payload.hasValue());
    EXPECT_EQ(payload.error(), DecryptionError::AuthenticationFailed);
}
