#pragma once

#include <optional>
#include <stdexcept>
#include <utility>
#include <variant>

namespace stapik
{
    template<typename ErrorType>
    class Failure
    {
    public:
        explicit Failure(ErrorType error) :
            m_error(std::move(error))
        {}

        [[nodiscard]] const ErrorType& error() const&
        {
            return m_error;
        }

        [[nodiscard]] ErrorType&& error() &&
        {
            return std::move(m_error);
        }

    private:
        ErrorType m_error;
    };

    class BadResultAccess : public std::logic_error
    {
    public:
        using std::logic_error::logic_error;
    };

    template<typename ValueType, typename ErrorType>
    class Result
    {
    public:
        Result(ValueType value) :
            m_state(std::in_place_index<VALUE_INDEX>, std::move(value))
        {}

        Result(Failure<ErrorType> failure) :
            m_state(std::in_place_index<FAILURE_INDEX>, std::move(failure))
        {}

        [[nodiscard]] bool hasValue() const
        {
            return m_state.index() == VALUE_INDEX;
        }

        explicit operator bool() const
        {
            return hasValue();
        }

        [[nodiscard]] ValueType& value() &
        {
            if (auto* stored = std::get_if<VALUE_INDEX>(&m_state))
                return *stored;
            throw BadResultAccess("Result holds an error, not a value");
        }

        [[nodiscard]] const ValueType& value() const&
        {
            if (const auto* stored = std::get_if<VALUE_INDEX>(&m_state))
                return *stored;
            throw BadResultAccess("Result holds an error, not a value");
        }

        [[nodiscard]] ValueType&& value() &&
        {
            return std::move(value());
        }

        [[nodiscard]] ValueType& operator*() &
        {
            return value();
        }

        [[nodiscard]] const ValueType& operator*() const&
        {
            return value();
        }

        [[nodiscard]] ValueType&& operator*() &&
        {
            return std::move(value());
        }

        [[nodiscard]] ValueType* operator->()
        {
            return &value();
        }

        [[nodiscard]] const ValueType* operator->() const
        {
            return &value();
        }

        template<typename FallbackType>
        [[nodiscard]] ValueType valueOr(FallbackType&& fallback) const&
        {
            return hasValue() ? value() : static_cast<ValueType>(std::forward<FallbackType>(fallback));
        }

        [[nodiscard]] const ErrorType& error() const&
        {
            if (const auto* stored = std::get_if<FAILURE_INDEX>(&m_state))
                return stored->error();
            throw BadResultAccess("Result holds a value, not an error");
        }

        [[nodiscard]] ErrorType&& error() &&
        {
            if (auto* stored = std::get_if<FAILURE_INDEX>(&m_state))
                return std::move(*stored).error();
            throw BadResultAccess("Result holds a value, not an error");
        }

    private:
        static constexpr std::size_t VALUE_INDEX = 0;
        static constexpr std::size_t FAILURE_INDEX = 1;

        std::variant<ValueType, Failure<ErrorType>> m_state;
    };

    // An operation that either succeeds without a result or fails with a reason.
    template<typename ErrorType>
    class Result<void, ErrorType>
    {
    public:
        Result() = default;

        Result(Failure<ErrorType> failure) :
            m_error(std::move(failure).error())
        {}

        [[nodiscard]] bool hasValue() const
        {
            return !m_error.has_value();
        }

        explicit operator bool() const
        {
            return hasValue();
        }

        [[nodiscard]] const ErrorType& error() const&
        {
            if (!m_error)
                throw BadResultAccess("Result holds a value, not an error");
            return *m_error;
        }

        [[nodiscard]] ErrorType&& error() &&
        {
            if (!m_error)
                throw BadResultAccess("Result holds a value, not an error");
            return std::move(*m_error);
        }

    private:
        std::optional<ErrorType> m_error;
    };
}
