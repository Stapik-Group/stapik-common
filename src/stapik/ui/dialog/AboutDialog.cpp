#include "AboutDialog.hpp"

#include "DialogUtils.hpp"
#include "StapikDialog.hpp"

#include "stapik/app/AppContext.hpp"
#include "stapik/locale/LocaleManager.hpp"

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/icontheme.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>
#include <gtkmm/linkbutton.h>

#include <string>

namespace
{
    constexpr int DIALOG_WIDTH = 380;
    constexpr int ICON_SIZE = 64;
    constexpr int BANNER_SPACING = 6;
    constexpr int BODY_SPACING = 14;
    constexpr int BODY_MARGIN = 16;
    constexpr int CARD_SPACING = 6;
    constexpr int CLOSE_BUTTON_WIDTH = 96;

    std::string displayNameOf(const stapik::app::AppInfo& appInfo)
    {
        return appInfo.displayName.empty() ? appInfo.internalName : appInfo.displayName;
    }

    class AboutDialog final : public StapikDialog
    {
    public:
        AboutDialog(Window& parent, const stapik::app::AppInfo& appInfo);

    private:
        Gtk::Box m_banner{ Gtk::Orientation::VERTICAL, BANNER_SPACING };
        Gtk::Image m_icon;
        Gtk::Label m_nameLabel;
        Gtk::Label m_versionLabel;
        Gtk::Box m_body{ Gtk::Orientation::VERTICAL, BODY_SPACING };
        Gtk::Box m_card{ Gtk::Orientation::VERTICAL, CARD_SPACING };
        Gtk::Label m_authorLabel;
        Gtk::LinkButton m_websiteLink;
        Gtk::Button m_closeButton;

        void initBanner(const stapik::app::AppInfo& appInfo, const std::string& name);
        void initCard(const stapik::app::AppInfo& appInfo);
        void initCloseButton();
        [[nodiscard]] std::string findIconName(const stapik::app::AppInfo& appInfo);
    };

    AboutDialog::AboutDialog(Window& parent, const stapik::app::AppInfo& appInfo) :
        StapikDialog(parent, LocaleManager::instance().translate("dialog.about.title", { { "name", displayNameOf(appInfo) } }))
    {

        add_css_class("stapik-about-dialog");
        set_default_size(DIALOG_WIDTH, -1);
        set_resizable(false);

        contentBox().set_margin(0);
        contentBox().set_spacing(0);

        initBanner(appInfo, displayNameOf(appInfo));
        initCard(appInfo);
        initCloseButton();

        m_body.set_margin(BODY_MARGIN);
        contentBox().append(m_banner);
        contentBox().append(m_body);
    }

    void AboutDialog::initBanner(const stapik::app::AppInfo& appInfo, const std::string& name)
    {
        m_banner.add_css_class("stapik-about-banner");
        m_banner.set_halign(Gtk::Align::FILL);

        if (const auto iconName = findIconName(appInfo); !iconName.empty())
        {
            m_icon.set_from_icon_name(iconName);
            m_icon.set_pixel_size(ICON_SIZE);
            m_icon.set_halign(Gtk::Align::CENTER);
            m_banner.append(m_icon);
        }

        m_nameLabel.set_text(name);
        m_nameLabel.add_css_class("stapik-about-name");
        m_nameLabel.set_halign(Gtk::Align::CENTER);
        m_banner.append(m_nameLabel);

        if (!appInfo.version.empty())
        {
            m_versionLabel.set_text(LocaleManager::instance().translate("dialog.about.version", { { "version", appInfo.version } }));
            m_versionLabel.add_css_class("stapik-about-version");
            m_versionLabel.set_halign(Gtk::Align::CENTER);
            m_banner.append(m_versionLabel);
        }
    }

    void AboutDialog::initCard(const stapik::app::AppInfo& appInfo)
    {
        const auto& loc = LocaleManager::instance();

        m_card.add_css_class("stapik-about-card");

        if (!appInfo.author.empty())
        {
            m_authorLabel.set_text(loc.translate("dialog.about.author", { { "author", appInfo.author } }));
            m_authorLabel.set_halign(Gtk::Align::CENTER);
            m_card.append(m_authorLabel);
        }

        if (!appInfo.repositoryUrl.empty())
        {
            m_websiteLink.set_uri(appInfo.repositoryUrl);
            m_websiteLink.set_label(loc.translate("dialog.about.website"));
            m_websiteLink.add_css_class("stapik-about-link");
            m_websiteLink.set_halign(Gtk::Align::CENTER);
            m_card.append(m_websiteLink);
        }

        m_card.set_visible(m_card.get_first_child() != nullptr);
        m_body.append(m_card);
    }

    void AboutDialog::initCloseButton()
    {
        m_closeButton.set_label(LocaleManager::instance().translate("dialog.button.ok"));
        m_closeButton.set_halign(Gtk::Align::CENTER);
        m_closeButton.set_size_request(CLOSE_BUTTON_WIDTH, -1);
        m_closeButton.signal_clicked().connect([this] { response(Gtk::ResponseType::OK); });
        m_body.append(m_closeButton);
        set_default_widget(m_closeButton);
    }

    std::string AboutDialog::findIconName(const stapik::app::AppInfo& appInfo)
    {
        const auto theme = Gtk::IconTheme::get_for_display(get_display());
        for (const auto& candidate : { appInfo.internalName, appInfo.applicationId })
        {
            if (!candidate.empty() && theme->has_icon(candidate))
                return candidate;
        }
        return {};
    }
}

void showAboutDialog(Gtk::Window& parent, const stapik::app::AppInfo& appInfo)
{
    showAutoDeletingDialog<AboutDialog>(parent, appInfo);
}

void showAboutDialog(Gtk::Window& parent)
{
    showAboutDialog(parent, stapik::app::AppContext::instance().info());
}
