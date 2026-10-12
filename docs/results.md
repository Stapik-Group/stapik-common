# Results and errors

`stapik::Result<ValueType, ErrorType>` (`stapik/core/Result.hpp`) is a small subset of `std::expected`, which C++20 does
not have. It holds either a value or the reason why there is none. `Result<void, ErrorType>` is for operations
that only succeed or fail. A failure is created with `Failure(error)`.

```cpp
stapik::storage::FileResult saved = documentFile.save(document);
if (!saved)
    showError(saved.error().message());
```

`value()` on a failure and `error()` on a success throw `BadResultAccess`; check first with `hasValue()` or `if`.

## Which return type to use

- `Result` when the caller can do something with the reason: show it, or tell a full disk from a missing permission.
- `std::optional` when "nothing" is the whole answer: parsing, looking a value up.
- `bool` for questions and for outcomes with no cause to report, such as `ObservableSetting::set()`, which says whether
  the value changed.
- Exceptions where a layer already defines them (`CloudStorageException`, `UpdateCheckException`).

## File errors

`storage::FileResult` is `Result<void, storage::FileError>`. `FileError` holds the path, the step that failed
(`"create directory"`, `"rename"`, ...) and a `std::error_code`, which can be compared with portable conditions:

```cpp
if (saved.error().code == std::errc::no_space_on_device)
    ...
```

These return it: `writeFileAtomically()`, `SettingsStore::save()` and `reset()`, `resetAppSettings()`,
`DocumentFile::save()` and `exportTo()`, `WindowState::save()` and `CloudStorageConfigStorage::save()`.
The failure is logged where it happens, so a caller that only checks the outcome does not hide it.
