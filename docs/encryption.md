# End-to-end encryption

When the cloud config has an `encryptionPassphrase`, documents are encrypted on the device before they are sent, so the
server only ever stores an opaque blob. The user types the passphrase in the connect dialog (optional field); it is kept
in the secret store next to the API key.

```cpp
CloudStorageConfig config{ .apiUrl = url, .apiKey = key, .encryptionPassphrase = passphrase };
auto storage = stapik::cloud::createCloudStorage(config, "budget");
```

`defaultCloudSessionHooks()` already goes through `createCloudStorage()`, so applications using `CloudSession` need no
changes.

## Where the secrets are kept

`CloudStorageConfigStorage` keeps the API key and the passphrase in the system secret store. When the store cannot be
read, `load()` returns the config with empty secrets and `secretsUnreadable` set, so the application can tell a locked
or unavailable keyring from a key that was never saved. Saving such a config leaves the stored secrets alone: a failed
read cannot delete them. A key the user types in the meantime still replaces the stored one.

Without a secret store (no Secret Service on Linux, for example) the secrets stay in `config.json`, readable only by the
user. `CloudStorageConfigStorage::secretsKeptInConfigFile(appName)` tells which of them are there, so the application
can warn about it after saving and in its settings.

```cpp
if (CloudStorageConfigStorage::secretsKeptInConfigFile(appName).any())
    showPlainTextSecretsWarning();
```

## What the server sees

| Visible to the server                     | Hidden from the server                                                       |
|-------------------------------------------|------------------------------------------------------------------------------|
| API key, slot key, partition names, sizes | document content                                                             |
| time of every write, number of partitions | `lastUpdate` of the document, titles, amounts, everything inside the payload |

## Blob format

```json
{ "format": "stapik-e2e", "version": 1, "salt": "...", "nonce": "...", "ciphertext": "..." }
```

- `version` tells what the decrypted bytes are: `1` is the JSON text of the document, `2` is a raw gzip stream (see
  [compression.md](compression.md)). The version is part of the authenticated data, so it cannot be switched without
  failing authentication.

- The key comes from the passphrase with Argon2id (libsodium, interactive limits) and the `salt` of the blob.
- The payload is sealed with XChaCha20-Poly1305. Any change to the blob, a wrong passphrase or a wrong context fails
  authentication.
- The slot key and the document or partition name are authenticated too, so the server cannot hand out the 2025
  partition as 2024 or the main document as a partition.
- Keys are derived once per salt and cached, so a sync does not pay for Argon2 on every request.

## Behavior to know about

- A failed decryption throws `CloudDecryptionException` (a `CloudStorageException`). The sync reports it like any other
  cloud failure and keeps the local document; nothing is written back while loading fails.
- Data saved before the passphrase was set is accepted as it is, with a warning in the log, and becomes encrypted on the
  next write. Until then the server could have replaced such data; this is the price of a painless switch-over.
- The passphrase cannot be recovered. Losing it means losing the cloud copy; the local document is not affected.
- Changing the passphrase on one device re-encrypts the cloud copy with the new one on that device's next write; the
  other devices cannot read it until they are given the same passphrase.
- Large documents are compressed before they are encrypted, and the gzip stream is encrypted as it is instead of as
  base64 text, so the data is base64 encoded once (about 1.33 times the gzip size) and not twice (about 1.78 times).
- An application built with an older stapik-common reports `version` 2 as data from a newer version of the
  application. Only large documents are affected (small ones keep `version` 1), but update all devices before
  turning on encryption for a large budget.
- Files sent through `CloudAssetClient` are not encrypted.
