#pragma once

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace stapik::ui
{
    template<typename ValueType>
    struct RadioOption
    {
        ValueType value;
        std::string id;
    };

    template<typename ValueType>
    class RadioOptions
    {
    public:
        RadioOptions() = default;

        explicit RadioOptions(std::vector<RadioOption<ValueType>> options) :
            m_options(std::move(options))
        {}

        [[nodiscard]] const std::string* idOf(const ValueType& value) const
        {
            const auto option = std::ranges::find(m_options, value, &RadioOption<ValueType>::value);
            return option == m_options.end() ? nullptr : &option->id;
        }

        [[nodiscard]] const ValueType* valueOf(const std::string_view id) const
        {
            const auto option = std::ranges::find_if(m_options, [id](const RadioOption<ValueType>& candidate)
            {
                return candidate.id == id;
            });

            return option == m_options.end() ? nullptr : &option->value;
        }

        [[nodiscard]] const std::vector<RadioOption<ValueType>>& all() const
        {
            return m_options;
        }

        [[nodiscard]] bool empty() const
        {
            return m_options.empty();
        }

    private:
        std::vector<RadioOption<ValueType>> m_options;
    };
}
