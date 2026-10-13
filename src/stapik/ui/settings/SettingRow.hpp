#pragma once

#include <gtkmm/widget.h>

namespace stapik::ui
{
    // How the settings window places the control of a row.
    enum class SettingRowLayout
    {
        // A drop-down, a number field or a font button: all of them are equally large.
        Compact,
        // A switch: it keeps its own size.
        Toggle,
        // A control that takes the room which is left, such as a path.
        Wide
    };

    class SettingRow
    {
    public:
        SettingRow() = default;
        virtual ~SettingRow() = default;

        SettingRow(const SettingRow&) = delete;
        SettingRow& operator=(const SettingRow&) = delete;

        [[nodiscard]] virtual Gtk::Widget& widget() = 0;
        virtual void retranslate() {}
        [[nodiscard]] virtual SettingRowLayout layout() const { return SettingRowLayout::Wide; }
    };
}
