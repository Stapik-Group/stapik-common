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
- `RequestBody` is `NoBody`, raw `Bytes` or one `MultipartFile`.
- `defaultHttpClient()` is libcurl restricted to `http` and `https`, TLS 1.2 or newer, without redirects.

## Where it is used

`CloudStorageClient` and `CloudAssetClient` take an `IHttpClient` as the last constructor argument, and so does
`createCloudStorage()`. Tests pass `FakeHttpClient` (`tests/support/FakeHttpClient.hpp`), queue the responses with
`respondWith()` or `failWith()` and read the sent requests from `requests`.
