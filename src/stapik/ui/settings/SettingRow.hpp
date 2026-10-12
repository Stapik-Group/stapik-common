#pragma once

#include <gtkmm/widget.h>

namespace stapik::ui
{
    class SettingRow
    {
    public:
        SettingRow() = default;
        virtual ~SettingRow() = default;

        SettingRow(const SettingRow&) = delete;
        SettingRow& operator=(const SettingRow&) = delete;

        [[nodiscard]] virtual Gtk::Widget& widget() = 0;
        virtual void retranslate() {}
    };
}
