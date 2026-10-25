# Compression of cloud documents

`createCloudStorage()` puts a `CompressedCloudStorage` in front of every cloud storage. A document whose JSON text is
at least 32 KiB is gzipped and sent as a small JSON blob; smaller documents are sent as they are.

```json
{ "format": "stapik-gzip", "version": 1, "data": "<base64 of the gzip stream>" }
```

- Compression runs before [encryption](encryption.md): encrypted data does not compress. When both are on, the
  encryption layer takes the gzip stream out of the blob and encrypts it directly, and puts the blob back after
  decrypting, so the stream is base64 encoded only once, in the encrypted blob. The stack is
  `CompressedCloudStorage` -> `EncryptedCloudStorage` (when a passphrase is set) -> `CloudStorageClient`.
- A document that would not get smaller (the base64 blob is not shorter than the text) is sent as it is.
- Partitions are handled like the main document.
- Reading accepts both forms, so uncompressed data written earlier keeps loading. Decompression is capped at 256 MiB.
- Damaged or unknown-version data throws `CloudStorageException`, which the sync reports like any other cloud failure.
- An application built with an older stapik-common cannot read a compressed document and reports it as unreadable,
  so update all devices before relying on large documents.
