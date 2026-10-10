#include "WindowState.hpp"

namespace
{
    bool isValidDimension(const int value)
    {
        return value >= stapik::ui::WindowState::MIN_DIMENSION && value <= stapik::ui::WindowState::MAX_DIMENSION;
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
}

namespace stapik::ui
{
    bool WindowState::hasSize() const
    {
        return isValidDimension(width) && isValidDimension(height);
    }

    std::optional<WindowState> WindowState::load(const settings::SettingsStore& store, const std::string& key)
    {
        if (!store.contains(widthKey(key)) && !store.contains(heightKey(key)) && !store.contains(maximizedKey(key)))
            return std::nullopt;

        WindowState state;

        const int width = store.get<int>(widthKey(key), 0);
        if (const int height = store.get<int>(heightKey(key), 0); isValidDimension(width) && isValidDimension(height))
        {
            state.width = width;
            state.height = height;
        }

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

        store.set<bool>(maximizedKey(key), maximized);
        return store.save();
    }
}
