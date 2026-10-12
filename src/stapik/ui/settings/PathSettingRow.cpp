#include "PathSettingRow.hpp"

#include "stapik/locale/LocaleManager.hpp"
#include "stapik/storage/PathText.hpp"
#include "stapik/ui/dialog/FileDialogs.hpp"

#include <gtkmm/eventcontrollerfocus.h>
#include <gtkmm/window.h>

#include <utility>

namespace
{
    constexpr int ROW_SPACING = 6;
    constexpr int ENTRY_WIDTH_CHARS = 32;
}

namespace stapik::ui
{
    PathSettingRow::PathSettingRow(settings::ObservableSetting<std::filesystem::path>& setting, const PathKind kind, std::string titleKey) :
        m_setting(setting),
        m_kind(kind),
        m_titleKey(std::move(titleKey)),
        m_box(Gtk::Orientation::HORIZONTAL, ROW_SPACING)
    {
        m_entry.set_text(storage::pathText(m_setting.get()));
        m_entry.set_hexpand(true);
        m_entry.set_width_chars(ENTRY_WIDTH_CHARS);

        m_box.append(m_entry);
        m_box.append(m_browseButton);
        retranslate();

        m_entry.signal_activate().connect([this] { commitEntry(); });

        const auto focusController = Gtk::EventControllerFocus::create();
        focusController->signal_leave().connect([this] { commitEntry(); });
        m_entry.add_controller(focusController);

        m_browseButton.signal_clicked().connect([this] { browse(); });
        m_settingConnection = m_setting.signalChanged().connect([this](const std::filesystem::path& path)
        {
            m_entry.set_text(storage::pathText(path));
        });
    }

    Gtk::Widget& PathSettingRow::widget()
    {
        return m_box;
    }

    void PathSettingRow::retranslate()
    {
        m_browseButton.set_label(LocaleManager::instance().translate("dialog.settings.browse"));
    }

    void PathSettingRow::commitEntry() const
    {
        m_setting.set(storage::pathFromText(m_entry.get_text()));
    }

    void PathSettingRow::browse()
    {
        auto* window = dynamic_cast<Gtk::Window*>(m_browseButton.get_root());
        if (window == nullptr)
            return;

        const FileDialogOptions options{ .title = LocaleManager::instance().translate(m_titleKey), .initialName = {} };
        auto onChosen = [&setting = m_setting](const std::filesystem::path& path) { setting.set(path); };

        if (m_kind == PathKind::Folder)
            showSelectFolderDialog(*window, options, onChosen);
        else
            showOpenFileDialog(*window, options, onChosen);
    }
}
