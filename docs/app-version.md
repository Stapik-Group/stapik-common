# Application name and version

`stapik_add_application()` generates `stapik/app/version.hpp` into the build directory and adds it to the include
path of the application. It replaces the `STAPIK_APP_NAME`, `STAPIK_APP_DISPLAY_NAME` and `STAPIK_APP_VERSION`
compile definitions.

```cpp
#include <stapik/app/version.hpp>

const stapik::app::AppInfo appInfo{
    .applicationId = "pl.stapik.Budgeting",
    .internalName = std::string{stapik::app::build::NAME},
    .displayName = std::string{stapik::app::build::DISPLAY_NAME},
    .version = std::string{stapik::app::build::VERSION},
};
```

| Constant                           | Taken from                                                   |
|------------------------------------|--------------------------------------------------------------|
| `stapik::app::build::NAME`         | `NAME`                                                       |
| `stapik::app::build::DISPLAY_NAME` | `DISPLAY_NAME`, `NAME` when it is not given                  |
| `stapik::app::build::VERSION`      | `VERSION`, the `VERSION` of the project when it is not given |

All three are `std::string_view` constants, so they can be used in constant expressions. The header is written at
configuration time; changing the version in `CMakeLists.txt` rebuilds only the files that include it.
