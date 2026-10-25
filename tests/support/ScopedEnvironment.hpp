#pragma once

#include "Environment.hpp"

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
                setEnvironment(m_name, *m_original);
            else
                unsetEnvironment(m_name);
        }

        ScopedEnvironment(const ScopedEnvironment&) = delete;
        ScopedEnvironment& operator=(const ScopedEnvironment&) = delete;

    private:
        std::string m_name;
        std::optional<std::string> m_original;
    };
}
