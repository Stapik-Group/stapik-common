#include "SecretStore.hpp"

#include "stapik/log/Log.hpp"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincred.h>

#include <system_error>
#elif defined(STAPIK_HAS_LIBSECRET)
#include <libsecret/secret.h>

#include <memory>
#endif

namespace stapik::storage
{
#if defined(_WIN32)
    namespace
    {
        std::wstring toWide(const std::string& utf8)
        {
            if (utf8.empty())
                return {};

            const auto length = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
            if (length <= 0)
                return {};

            std::wstring wide(static_cast<std::size_t>(length), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), wide.data(), length);
            return wide;
        }

        std::wstring targetName(const std::string& service, const std::string& account)
        {
            return toWide(service + "/" + account);
        }

        // Credential Manager encrypts the blob with DPAPI, so only the same Windows user can read it back.
        class CredentialManagerSecretStore final : public ISecretStore
        {
        public:
            bool store(const std::string& service, const std::string& account, const std::string& secret) override
            {
                auto target = targetName(service, account);
                auto userName = toWide(account);

                CREDENTIALW credential{};
                credential.Type = CRED_TYPE_GENERIC;
                credential.TargetName = target.data();
                credential.UserName = userName.data();
                credential.CredentialBlobSize = static_cast<DWORD>(secret.size());
                credential.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char*>(secret.data()));
                credential.Persist = CRED_PERSIST_LOCAL_MACHINE;

                if (CredWriteW(&credential, 0))
                    return true;

                log::warning("Cannot write credential {}/{}: {}", service, account, std::system_category().message(static_cast<int>(GetLastError())));
                return false;
            }

            std::optional<std::string> lookup(const std::string& service, const std::string& account) override
            {
                PCREDENTIALW credential = nullptr;
                if (!CredReadW(targetName(service, account).c_str(), CRED_TYPE_GENERIC, 0, &credential))
                {
                    const auto errorCode = GetLastError();
                    if (errorCode != ERROR_NOT_FOUND)
                        log::warning("Cannot read credential {}/{}: {}", service, account, std::system_category().message(static_cast<int>(errorCode)));
                    return std::nullopt;
                }

                std::string secret(reinterpret_cast<const char*>(credential->CredentialBlob), credential->CredentialBlobSize);
                CredFree(credential);
                return secret;
            }

            bool remove(const std::string& service, const std::string& account) override
            {
                if (CredDeleteW(targetName(service, account).c_str(), CRED_TYPE_GENERIC, 0))
                    return true;

                const auto errorCode = GetLastError();
                if (errorCode != ERROR_NOT_FOUND)
                    log::warning("Cannot delete credential {}/{}: {}", service, account, std::system_category().message(static_cast<int>(errorCode)));
                return false;
            }
        };
    }

    ISecretStore& platformSecretStore()
    {
        static CredentialManagerSecretStore instance;
        return instance;
    }
#elif defined(STAPIK_HAS_LIBSECRET)
    namespace
    {
        constexpr auto SERVICE_ATTRIBUTE = "service";
        constexpr auto ACCOUNT_ATTRIBUTE = "account";

        struct SchemaDeleter
        {
            void operator()(SecretSchema* schema) const
            {
                secret_schema_unref(schema);
            }
        };

        const SecretSchema* secretSchema()
        {
            static const std::unique_ptr<SecretSchema, SchemaDeleter> schema{
                secret_schema_new("pl.stapik.Secret", SECRET_SCHEMA_NONE,
                    SERVICE_ATTRIBUTE, SECRET_SCHEMA_ATTRIBUTE_STRING,
                    ACCOUNT_ATTRIBUTE, SECRET_SCHEMA_ATTRIBUTE_STRING,
                    nullptr)
            };
            return schema.get();
        }

        void logFailure(const char* action, const std::string& service, const std::string& account, GError* error)
        {
            if (error == nullptr)
                return;

            log::warning("Cannot {} secret {}/{}: {}", action, service, account, error->message);
            g_error_free(error);
        }

        class LibsecretSecretStore final : public ISecretStore
        {
        public:
            bool store(const std::string& service, const std::string& account, const std::string& secret) override
            {
                GError* error = nullptr;
                const auto label = service + ": " + account;

                const bool stored = secret_password_store_sync(secretSchema(), SECRET_COLLECTION_DEFAULT, label.c_str(), secret.c_str(),
                    nullptr, &error,
                    SERVICE_ATTRIBUTE, service.c_str(),
                    ACCOUNT_ATTRIBUTE, account.c_str(),
                    nullptr);

                logFailure("store", service, account, error);
                return stored;
            }

            std::optional<std::string> lookup(const std::string& service, const std::string& account) override
            {
                GError* error = nullptr;

                gchar* password = secret_password_lookup_sync(secretSchema(), nullptr, &error,
                    SERVICE_ATTRIBUTE, service.c_str(),
                    ACCOUNT_ATTRIBUTE, account.c_str(),
                    nullptr);

                logFailure("read", service, account, error);
                if (password == nullptr)
                    return std::nullopt;

                std::string secret(password);
                secret_password_free(password);
                return secret;
            }

            bool remove(const std::string& service, const std::string& account) override
            {
                GError* error = nullptr;

                const bool removed = secret_password_clear_sync(secretSchema(), nullptr, &error,
                    SERVICE_ATTRIBUTE, service.c_str(),
                    ACCOUNT_ATTRIBUTE, account.c_str(),
                    nullptr);

                logFailure("delete", service, account, error);
                return removed;
            }
        };
    }

    ISecretStore& platformSecretStore()
    {
        static LibsecretSecretStore instance;
        return instance;
    }
#else
    namespace
    {
        class UnavailableSecretStore final : public ISecretStore
        {
        public:
            bool store(const std::string&, const std::string&, const std::string&) override
            {
                return false;
            }

            std::optional<std::string> lookup(const std::string&, const std::string&) override
            {
                return std::nullopt;
            }

            bool remove(const std::string&, const std::string&) override
            {
                return false;
            }
        };
    }

    ISecretStore& platformSecretStore()
    {
        static UnavailableSecretStore instance;
        return instance;
    }
#endif
}
