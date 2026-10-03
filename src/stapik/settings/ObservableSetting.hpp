#pragma once

#include "SettingsStore.hpp"

#include "stapik/log/Log.hpp"

#include <sigc++/signal.h>

#include <concepts>
#include <string>
#include <utility>

namespace stapik::settings
{
    template<std::equality_comparable ValueType>
    class ObservableSetting
    {
    public:
        ObservableSetting(SettingsStore& store, std::string key, ValueType defaultValue) :
            m_store(store),
            m_key(std::move(key)),
            m_defaultValue(std::move(defaultValue)),
            m_value(m_store.get(m_key, m_defaultValue))
        {}

        ObservableSetting(const ObservableSetting&) = delete;
        ObservableSetting& operator=(const ObservableSetting&) = delete;

        [[nodiscard]] const ValueType& get() const
        {
            return m_value;
        }

        [[nodiscard]] const ValueType& defaultValue() const
        {
            return m_defaultValue;
        }

        bool set(const ValueType& newValue)
        {
            if (newValue == m_value)
                return false;

            m_value = newValue;
            m_store.set(m_key, m_value);

            if (!m_store.save())
                log::warning("Cannot persist setting '{}'", m_key);

            m_signalChanged.emit(m_value);
            return true;
        }

        bool reset()
        {
            return set(m_defaultValue);
        }

        void reload()
        {
            const auto reloaded = m_store.get(m_key, m_defaultValue);
            if (reloaded == m_value)
                return;

            m_value = reloaded;
            m_signalChanged.emit(m_value);
        }

        sigc::signal<void(const ValueType&)>& signalChanged()
        {
            return m_signalChanged;
        }

    private:
        SettingsStore& m_store;
        std::string m_key;
        ValueType m_defaultValue;
        ValueType m_value;
        sigc::signal<void(const ValueType&)> m_signalChanged;
    };
}
