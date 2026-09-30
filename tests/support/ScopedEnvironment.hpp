#pragma once

#include <cstdlib>
#include <optional>
#include <string>
#include <utility>

namespace stapik::test
{
    class ScopedEnvironment
    {
    public:
        explicit ScopedEnvironment(std::string name) : m_name(std::move(name))
        {
            if (const auto* value = std::getenv(m_name.c_str()))
                m_original = value;
        }

        ~ScopedEnvironment()
        {
            if (m_original)
                setenv(m_name.c_str(), m_original->c_str(), 1);
            else
                unsetenv(m_name.c_str());
        }

        ScopedEnvironment(const ScopedEnvironment&) = delete;
        ScopedEnvironment& operator=(const ScopedEnvironment&) = delete;

    private:
        std::string m_name;
        std::optional<std::string> m_original;
    };
}
