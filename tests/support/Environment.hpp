#pragma once

#include <cstdlib>
#include <string>

namespace stapik::test
{
    inline void setEnvironment(const std::string& name, const std::string& value)
    {
#ifdef _WIN32
        _putenv_s(name.c_str(), value.c_str());
#else
        setenv(name.c_str(), value.c_str(), 1);
#endif
    }

    inline void unsetEnvironment(const std::string& name)
    {
#ifdef _WIN32
        _putenv_s(name.c_str(), "");
#else
        unsetenv(name.c_str());
#endif
    }
}
