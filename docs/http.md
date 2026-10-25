# HTTP client

`stapik::http::IHttpClient` (`stapik/http/IHttpClient.hpp`) sends one blocking HTTP request and returns the response.
It is the only place that talks to libcurl, so everything built on it can be tested without a network.

```cpp
stapik::http::Request request{
    .method = stapik::http::Method::Put,
    .url = "https://cloud.example/api/v1/documents/budget",
    .headers = { { "x-api-key", key } },
    .body = stapik::http::toBytes(json.dump()),
    .timeout = std::chrono::seconds(8) };

const auto response = stapik::http::defaultHttpClient().send(request);
if (!response)
    showError(response.error().message);
```

- Any response is a success, whatever its status. Only a request that got no response at all (connection, TLS,
  timeout, a body above `maxResponseBytes`) is a `TransportError`, returned as a `Result` (see [results.md](results.md)).
- A request has three time limits: `connectTimeout` (10 s) for establishing the connection, `stallTimeout` (30 s) for
  a transfer that has stopped moving data, and `timeout` (30 s) for the whole request. `NO_TIMEOUT` removes the
  overall limit, which is what large transfers such as asset uploads use, so that a slow but alive connection is
  not cut off.
- `onProgress` receives the bytes sent and received so far, and `isCancelled` is polled during the transfer; returning
  `true` aborts it with a `TransportError` whose `cancelled` is set. Both run on the thread that called `send()`.
  The cloud clients fill `isCancelled` themselves, so a request ends early when the `BackgroundTaskRunner` it runs on
  is destroyed (see [cloud.md](cloud.md)).
- `Response::retryAfter` is the parsed `Retry-After` header (seconds, or an HTTP date turned into a delay).
  `defaultHttpClient()` honours it: on 429 or 503 it waits for the requested time and sends the request again, at most
  twice and only when the wait is 30 s or less, otherwise the response is returned as it is. A `POST` or `PATCH` is
  sent again only after a 429: a 503 does not tell whether the request was already processed. The wait ends early with
  a cancelled `TransportError` when `isCancelled` says so. `RetryingHttpClient` can wrap any other `IHttpClient`.
- `Response::headers` holds the headers of the final response with lower-case names (`Response::header("etag")`); a
  repeated header keeps its last value.
- Proxy: libcurl follows `http_proxy`, `https_proxy` and `all_proxy` on every platform. On Windows, when none of them
  is set, the proxy of the Windows user settings is used: a configuration script (PAC) or auto-detection, otherwise
  the fixed proxy per scheme, with the bypass list. The `<local>` entry of the bypass list has no libcurl equivalent
  and is ignored. NTLM and Negotiate proxies are signed in to with the current Windows user. The answer is cached
  for a minute per host.
- Every thread keeps one libcurl handle and resets it after each request, so consecutive requests of a thread reuse
  the connection, the TLS session and the DNS answer. The requests of the shared background pool (see
  [cloud.md](cloud.md)) therefore talk to the cloud over a few warm connections instead of a new handshake each. A
  request made while the thread's handle is busy (from inside a progress callback) gets a handle of its own.
- `RequestBody` is `NoBody`, raw `Bytes` or one `MultipartFile`.
- `defaultHttpClient()` is libcurl restricted to `http` and `https`, TLS 1.2 or newer, without redirects.
- It identifies itself with `User-Agent: <internalName>/<version>` taken from `AppInfo` (see `app::userAgentOf()`),
  or `stapik-common` before `AppContext` is initialized. A `User-Agent` header set on the request takes precedence.

## Where it is used

`CloudStorageClient` and `CloudAssetClient` take an `IHttpClient` as the last constructor argument, and so does
`createCloudStorage()`. Tests pass `FakeHttpClient` (`tests/support/FakeHttpClient.hpp`), queue the responses with
`respondWith()`, `respondWithHeaders()` or `failWith()` and read the sent requests from `requests`.
