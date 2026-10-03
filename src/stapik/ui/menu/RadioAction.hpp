#pragma once

#include "RadioOptions.hpp"

#include "stapik/log/Log.hpp"

#include <giomm/actionmap.h>
#include <giomm/simpleaction.h>
#include <glibmm/ustring.h>
#include <glibmm/variant.h>

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace stapik::ui
{
    template<typename ValueType>
    class RadioAction
    {
    public:
        using Option = RadioOption<ValueType>;
        using ChangedHandler = std::function<void(const ValueType&)>;

        RadioAction(
            Gio::ActionMap& actionMap,
            std::string name,
            std::vector<Option> options,
            const ValueType& initialValue,
            ChangedHandler onChanged,
            std::string actionPrefix = "win") :
            m_name(std::move(name)),
            m_actionPrefix(std::move(actionPrefix)),
            m_state(std::make_shared<State>(RadioOptions<ValueType>(std::move(options)), std::move(onChanged), initialValue))
        {
            const auto* initialId = m_state->options.idOf(initialValue);

            if (initialId == nullptr && !m_state->options.empty())
            {
                log::warning("Initial value of radio action '{}' is not among its options, using the first one", m_name);
                m_state->current = m_state->options.all().front().value;
                initialId = m_state->options.idOf(m_state->current);
            }

            m_action = Gio::SimpleAction::create_radio_string(m_name, Glib::ustring(initialId != nullptr ? *initialId : std::string()));

            auto* rawAction = m_action.get();
            const auto state = m_state;

            m_action->signal_activate().connect([rawAction, state](const Glib::VariantBase& parameter)
            {
                const auto id = Glib::VariantBase::cast_dynamic<Glib::Variant<Glib::ustring>>(parameter).get();

                const auto* value = state->options.valueOf(id.raw());
                if (value == nullptr)
                    return;

                rawAction->change_state(id);
                state->current = *value;

                if (state->handler)
                    state->handler(*value);
            });

            actionMap.add_action(m_action);
        }

        void setValue(const ValueType& value)
        {
            const auto* id = m_state->options.idOf(value);
            if (id == nullptr)
                return;

            m_action->change_state(Glib::ustring(*id));
            m_state->current = value;
        }

        [[nodiscard]] const ValueType& value() const
        {
            return m_state->current;
        }

        [[nodiscard]] std::string detailedAction(const ValueType& value) const
        {
            const auto* id = m_state->options.idOf(value);
            if (id == nullptr)
                return {};

            return m_actionPrefix + "." + m_name + "::" + *id;
        }

    private:
        struct State
        {
            State(RadioOptions<ValueType> stateOptions, ChangedHandler stateHandler, const ValueType& initialValue) :
                options(std::move(stateOptions)),
                handler(std::move(stateHandler)),
                current(initialValue)
            {}

            RadioOptions<ValueType> options;
            ChangedHandler handler;
            ValueType current;
        };

        std::string m_name;
        std::string m_actionPrefix;
        std::shared_ptr<State> m_state;
        Glib::RefPtr<Gio::SimpleAction> m_action;
    };
}
