#include "WindowState.hpp"

#include <limits>

namespace
{
    constexpr int MISSING_COORDINATE = std::numeric_limits<int>::min();

    bool isValidDimension(const int value)
    {
        return value >= stapik::ui::WindowState::MIN_DIMENSION && value <= stapik::ui::WindowState::MAX_DIMENSION;
    }

    bool isValidCoordinate(const int value)
    {
        return value >= -stapik::ui::WindowState::MAX_COORDINATE && value <= stapik::ui::WindowState::MAX_COORDINATE;
    }

    std::string widthKey(const std::string& key)
    {
        return key + ".width";
    }

    std::string heightKey(const std::string& key)
    {
        return key + ".height";
    }

    std::string maximizedKey(const std::string& key)
    {
        return key + ".maximized";
    }

    std::string positionXKey(const std::string& key)
    {
        return key + ".x";
    }

    std::string positionYKey(const std::string& key)
    {
        return key + ".y";
    }

    bool hasAnyStoredValue(const stapik::settings::SettingsStore& store, const std::string& key)
    {
        return store.contains(widthKey(key))
            || store.contains(heightKey(key))
            || store.contains(maximizedKey(key))
            || store.contains(positionXKey(key))
            || store.contains(positionYKey(key));
    }
}

namespace stapik::ui
{
    bool WindowState::hasSize() const
    {
        return isValidDimension(width) && isValidDimension(height);
    }

    bool WindowState::hasPosition() const
    {
        return position.has_value() && isValidCoordinate(position->x) && isValidCoordinate(position->y);
    }

    std::optional<WindowState> WindowState::load(const settings::SettingsStore& store, const std::string& key)
    {
        if (!hasAnyStoredValue(store, key))
            return std::nullopt;

        WindowState state;

        const int width = store.get<int>(widthKey(key), 0);
        if (const int height = store.get<int>(heightKey(key), 0); isValidDimension(width) && isValidDimension(height))
        {
            state.width = width;
            state.height = height;
        }

        const int x = store.get<int>(positionXKey(key), MISSING_COORDINATE);
        if (const int y = store.get<int>(positionYKey(key), MISSING_COORDINATE); isValidCoordinate(x) && isValidCoordinate(y))
            state.position = WindowPosition{ .x = x, .y = y };

        state.maximized = store.get<bool>(maximizedKey(key), false);
        return state;
    }

    bool WindowState::save(settings::SettingsStore& store, const std::string& key) const
    {
        if (hasSize())
        {
            store.set<int>(widthKey(key), width);
            store.set<int>(heightKey(key), height);
        }
        else
        {
            store.remove(widthKey(key));
            store.remove(heightKey(key));
        }

        if (hasPosition())
        {
            store.set<int>(positionXKey(key), position->x);
            store.set<int>(positionYKey(key), position->y);
        }
        else
        {
            store.remove(positionXKey(key));
            store.remove(positionYKey(key));
        }

        store.set<bool>(maximizedKey(key), maximized);
        return store.save();
    }
}
