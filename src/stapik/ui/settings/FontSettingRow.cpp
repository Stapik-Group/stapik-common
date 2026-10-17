#include "FontSettingRow.hpp"

#include "stapik/locale/LocaleManager.hpp"
#include "stapik/theme/FontChoice.hpp"

#include <glibmm/refptr.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/fontdialog.h>
#include <pango/pango.h>
#include <pangomm/context.h>
#include <pangomm/fontdescription.h>

#include <algorithm>
#include <cmath>
#include <string>

namespace
{
    constexpr int BOX_SPACING = 6;
    constexpr double SIZE_ROUNDING_STEP = 0.5;
    constexpr double SIZE_SPIN_PAGE_STEP = 1.0;
    constexpr int SIZE_SPIN_DIGITS = 1;
    constexpr int SIZE_SPIN_WIDTH_CHARS = 5;
    constexpr double FALLBACK_SIZE_POINTS = 10.0;
    constexpr auto FALLBACK_FAMILY = "Sans";

    double sizeInPoints(const Pango::FontDescription& description)
    {
        const double size = static_cast<double>(description.get_size()) / PANGO_SCALE;
        // An absolute size is in device units, which are the pixels of the CSS.
        return pango_font_description_get_size_is_absolute(description.gobj()) ? size / stapik::theme::CSS_PIXELS_PER_POINT : size;
    }

    // The font is a list in the CSS of a theme; only its first family is shown.
    std::string firstFamily(const std::string& families)
    {
        auto family = families.substr(0, families.find(','));

        const auto begin = family.find_first_not_of(" \"'");
        if (begin == std::string::npos)
            return {};

        return family.substr(begin, family.find_last_not_of(" \"'") - begin + 1);
    }
}

namespace stapik::ui
{
    FontSettingRow::FontSettingRow(settings::ObservableSetting<std::string>& setting) :
        m_setting(setting),
        m_box(Gtk::Orientation::HORIZONTAL, BOX_SPACING)
    {
        const auto dialog = Gtk::FontDialog::create();
        dialog->set_modal(true);
        m_button.set_dialog(dialog);
        m_button.set_use_font(true);
        m_button.set_hexpand(true);

        m_sizeSpin.set_adjustment(Gtk::Adjustment::create(
            FALLBACK_SIZE_POINTS, theme::MIN_FONT_SIZE_POINTS, theme::MAX_FONT_SIZE_POINTS, SIZE_ROUNDING_STEP, SIZE_SPIN_PAGE_STEP));
        m_sizeSpin.set_digits(SIZE_SPIN_DIGITS);
        m_sizeSpin.set_width_chars(SIZE_SPIN_WIDTH_CHARS);

        m_box.append(m_button);
        m_box.append(m_sizeSpin);
        m_box.append(m_resetButton);

        m_button.property_font_desc().signal_changed().connect([this] { onFontPicked(); });
        m_sizeSpin.signal_value_changed().connect([this] { onSizeEdited(); });
        m_resetButton.signal_clicked().connect([this] { m_setting.reset(); });
        m_settingConnection = m_setting.signalChanged().connect([this](const std::string&) { showSetting(); });

        retranslate();
    }

    Gtk::Widget& FontSettingRow::widget()
    {
        return m_box;
    }

    SettingRowLayout FontSettingRow::layout() const
    {
        return SettingRowLayout::Compact;
    }

    void FontSettingRow::retranslate()
    {
        const auto& loc = LocaleManager::instance();

        m_button.get_dialog()->set_title(loc.translate("dialog.font.title"));
        m_resetButton.set_label(loc.translate("settings.font.default"));
        m_sizeSpin.set_tooltip_text(loc.translate("settings.font.size"));
        showSetting();
    }

    void FontSettingRow::showSetting()
    {
        const auto choice = theme::parseFontChoice(m_setting.get());
        m_resetButton.set_visible(choice.has_value());

        const auto description = choice ? Pango::FontDescription(theme::formatFontChoice(*choice)) : themeFont();

        m_programmaticChange = true;
        m_button.set_font_desc(description);
        m_sizeSpin.set_value(std::clamp(
            std::round(sizeInPoints(description) / SIZE_ROUNDING_STEP) * SIZE_ROUNDING_STEP,
            theme::MIN_FONT_SIZE_POINTS, theme::MAX_FONT_SIZE_POINTS));
        m_programmaticChange = false;
    }

    void FontSettingRow::onFontPicked()
    {
        if (m_programmaticChange)
            return;

        const auto description = m_button.get_font_desc();
        const std::string family = firstFamily(description.get_family());
        if (family.empty())
            return;

        const auto size = std::round(sizeInPoints(description) / SIZE_ROUNDING_STEP) * SIZE_ROUNDING_STEP;
        m_setting.set(theme::formatFontChoice({
            .family = family,
            .sizePoints = std::clamp(size, theme::MIN_FONT_SIZE_POINTS, theme::MAX_FONT_SIZE_POINTS) }));
    }

    void FontSettingRow::onSizeEdited()
    {
        if (m_programmaticChange)
            return;

        // Only the size changes; the family stays the one that is shown on the button.
        const std::string family = firstFamily(m_button.get_font_desc().get_family());
        if (family.empty())
            return;

        m_setting.set(theme::formatFontChoice({ .family = family, .sizePoints = m_sizeSpin.get_value() }));
    }

    // What the application uses now, as far as the widget can tell; it is shown while the theme's font is in use.
    Pango::FontDescription FontSettingRow::themeFont()
    {
        std::string family = FALLBACK_FAMILY;
        double size = FALLBACK_SIZE_POINTS;

        if (const auto context = m_button.get_pango_context())
        {
            const auto current = context->get_font_description();
            if (const auto first = firstFamily(current.get_family()); !first.empty())
                family = first;

            if (const auto points = sizeInPoints(current); points > 0.0)
                size = std::round(points / SIZE_ROUNDING_STEP) * SIZE_ROUNDING_STEP;
        }

        return Pango::FontDescription(theme::formatFontChoice({ .family = family, .sizePoints = size }));
    }
}
