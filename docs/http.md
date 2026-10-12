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
- `RequestBody` is `NoBody`, raw `Bytes` or one `MultipartFile`.
- `defaultHttpClient()` is libcurl restricted to `http` and `https`, TLS 1.2 or newer, without redirects.
- It identifies itself with `User-Agent: <internalName>/<version>` taken from `AppInfo` (see `app::userAgentOf()`),
  or `stapik-common` before `AppContext` is initialized. A `User-Agent` header set on the request takes precedence.

## Where it is used

`CloudStorageClient` and `CloudAssetClient` take an `IHttpClient` as the last constructor argument, and so does
`createCloudStorage()`. Tests pass `FakeHttpClient` (`tests/support/FakeHttpClient.hpp`), queue the responses with
`respondWith()` or `failWith()` and read the sent requests from `requests`.
