# Cloud client

stapik-common 1.6.0 and newer talk to **stapik-cloud 1.2.0 or newer** (app API 1.0.3). Older clouds are not supported:
an application that has to work with one stays on an older stapik-common.

## Documents and conflicts

`CloudStorageClient` implements `ICloudStorage` (`stapik/cloud/ICloudStorage.hpp`) for one slot.

- The cloud detects conflicts by version: every document has an `ETag` (the quoted SHA-256 of its content), a `PUT`
  carries it in `If-Match`, and a missing or stale one answers `409` with the current document.
- The sync keeps a timestamp baseline (`lastKnownCloudUpdate`). The client translates it: before a save it knows the
  ETag of the version the baseline belongs to (from an earlier read or save, remembered per URL) or reads the document
  once to get it. A baseline that does not match the current `updatedAt` is a conflict without sending the `PUT`.
- Both a `409` and such a stale baseline come back as `CloudWriteResult::conflict` with the cloud's own version.
- Saving the same content again is answered `200` with the existing version, nothing new is stored.
- Deleting a partition is soft: it reads as missing and disappears from the partition list.

## Files (assets)

`CloudAssetClient` implements `IAssetStorage`.

- File names are normalized to NFC and checked like the cloud does (at most 255 bytes of UTF-8, none of
  `/ \ : * ? " < > |`, no control characters, no leading whitespace, no trailing space or dot, no reserved device
  names such as `CON` or `LPT1`). A bad name throws `InvalidAssetFilenameException` before anything is sent.
- Every upload carries `X-Checksum-Sha256`. A refusal is reported as `AssetTooLargeException` (413),
  `AssetTypeNotAllowedException` (415, the slot does not accept that kind of file) or
  `AssetChecksumMismatchException` (422, the file was damaged on the way).
- A file of 32 MiB or more is uploaded in 8 MiB chunks through `/asset-uploads` (`AssetChunking` changes both
  numbers). A chunk that did not arrive is found out by asking the cloud for the offset and sent again, at most three
  interruptions in a row. A cancelled or failed upload is aborted in the cloud. An upload stopped by a long
  `CloudRateLimitedException` is kept: uploading the same file again continues from the offset the cloud reports,
  or starts anew when the cloud no longer has the upload.
- Downloads are limited to 64 MiB. A slot that accepts larger files needs its `AssetSizeLimit` as the last argument of
  `CloudAssetClient`: the limit is then the larger of the two, so every file that could be uploaded can be downloaded
  (and backed up). A file above the limit fails with "The response is larger than the limit of N bytes".

## Limits and warnings

- At most 600 requests a minute per API key. An answer `429` with a `Retry-After` of 30 s or less is waited out by
  the HTTP client (see [http.md](http.md)); otherwise `CloudRateLimitedException::retryAfter()` carries the wait,
  and `rateLimit()` the allowance the answer reported (`X-RateLimit-Limit`, `-Remaining`, `-Reset`, see
  `parseRateLimit()`). The sync keeps its local document and does not retry sooner than that (`SyncOutcome::retryAfter`).
- When the API key is about to expire (30 days by default) the cloud adds `X-Api-Key-Expires-At` to its answers.
  `ApiKeyExpiryMonitor` records it, and `CloudSession::signalApiKeyExpiring` reports it once per date. The text to show
  is the translation key `cloud.apiKeyExpiring` with the argument `{date}`.

## Connecting and disconnecting

- `CloudSession::connectWith()` and `connectPartitionsOnly()` save the configuration through `hooks.saveConfig` and
  connect. `disconnect()` is the way back: it forgets the connection and calls `hooks.clearConfig`, which in
  `defaultCloudSessionHooks()` removes the saved configuration, the API key and the passphrase
  (`CloudStorageConfigStorage::clear()`), so the next start does not connect again. Local documents are not touched.
- A new connection and `disconnect()` first send what is still pending to the storage they leave, waiting at most
  `CloudSession::DRAIN_TIMEOUT` (3 s). What could not be sent in that time stays in the local document, which the sync
  reconciles with the cloud on the next connection.

## Background work

- Sync jobs, the fetch of the available years and every other `BackgroundTaskRunner` task run on one pool of at most
  four threads shared by the whole process. A scheduler per partition therefore does not mean a thread per partition:
  an idle scheduler holds no thread, and jobs of many partitions wait for a free one. `CloudSession::forgetPartition()`
  also releases the scheduler of the partition as soon as it has nothing left to send.
- A `BackgroundTaskRunner` still runs its own tasks one after another, in the order they were submitted, and drops the
  ones that have not started when it is destroyed.
- Destroying a `BackgroundTaskRunner`, and so an `AsyncSyncScheduler` or a `CloudSession`, also asks the running task
  to stop instead of waiting for it. `CloudStorageClient` and `CloudAssetClient` poll `task::isCancellationRequested()`
  during a request, so closing the application or connecting again does not wait for a request to time out: the request
  ends with `CloudRequestCancelledException` (`AssetTransferCancelledException` for a file). A task of your own can poll
  `task::isCancellationRequested()` the same way, and `task::CancellationScope` lets a thread that is not run by a
  `BackgroundTaskRunner` hand it a `std::stop_token`.
- Because destroying the session cancels what is still running, call `CloudSession::flushAndWait()` when the
  application closes: it sends the pending changes of the document and of every partition at once and waits for them,
  at most 3 s unless another timeout is passed. `AsyncSyncScheduler::drain()` does the same for one scheduler. The
  outcome of that last send is not reported through the signals, since nothing is left to show it.

## Change feed

`ICloudStorage::listChanges(since)` returns what changed in any slot of the key's extension after `since`, deleted
documents included, without their contents. Pass the `serverTime` of the answer as `since` of the next call. The
sync does not poll it yet.

## End-to-end encryption

The content sent to the cloud for an encrypted document has `"format": "stapik-e2e"` at the top level, which is how
the admin panel knows to show it read-only (see [encryption.md](encryption.md)). A test pins it, because it depends on
the order of the layers: compression, then encryption.
