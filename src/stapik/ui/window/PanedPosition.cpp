#include "PanedPosition.hpp"

#include "stapik/log/Log.hpp"

namespace
{
    constexpr int MISSING_POSITION = -1;

    bool isValidPosition(const int position)
    {
        return position >= 0 && position <= stapik::ui::MAX_PANED_POSITION;
    }
}

namespace stapik::ui
{
    std::optional<int> loadPanedPosition(const settings::SettingsStore& store, const std::string& key)
    {
        const int position = store.get<int>(key, MISSING_POSITION);
        return isValidPosition(position) ? std::optional(position) : std::nullopt;
    }

    bool savePanedPosition(settings::SettingsStore& store, const std::string& key, const int position)
    {
        if (!isValidPosition(position))
            return false;

        store.set<int>(key, position);
        return store.save();
    }

    void rememberPanedPosition(Gtk::Paned& paned, settings::SettingsStore& store, const std::string& key)
    {
        if (const auto position = loadPanedPosition(store, key))
            paned.set_position(*position);

        paned.signal_unmap().connect([&paned, &store, key]
        {
            if (!savePanedPosition(store, key, paned.get_position()))
                log::warning("Cannot persist the position of paned '{}'", key);
        });
    }
}
