#include "ConnectDialog.hpp"

#include "stapik/locale/LocaleManager.hpp"

ConnectDialog::ConnectDialog(Window& parent) :
    StapikDialog(parent, LocaleManager::instance().translate("dialog.connect.title"))
{
    initLayout();
}

void ConnectDialog::initLayout()
{
    const auto& loc = LocaleManager::instance();

    m_apiUrlLabel.set_text(loc.translate("dialog.connect.url.label"));
    m_apiUrlLabel.set_halign(Gtk::Align::START);
    m_apiUrlEntry.set_placeholder_text("https://...");

    m_apiKeyLabel.set_text(loc.translate("dialog.connect.key.label"));
    m_apiKeyLabel.set_halign(Gtk::Align::START);
    m_apiKeyEntry.set_placeholder_text(loc.translate("dialog.connect.key.placeholder"));
    m_apiKeyEntry.set_visibility(false);

    m_apiUrlHintLabel.set_halign(Gtk::Align::START);
    m_apiUrlHintLabel.set_wrap(true);
    m_apiUrlHintLabel.set_visible(false);

    m_showKeyCheck.set_label(loc.translate("dialog.connect.key.show"));
    m_showKeyCheck.signal_toggled().connect([this]
    {
        m_apiKeyEntry.set_visibility(m_showKeyCheck.get_active());
    });

    m_apiUrlEntry.signal_changed().connect([this] { updateValidation(); });
    m_apiKeyEntry.signal_changed().connect([this] { updateValidation(); });

    contentBox().append(m_apiUrlLabel);
    contentBox().append(m_apiUrlEntry);
    contentBox().append(m_apiUrlHintLabel);
    contentBox().append(m_apiKeyLabel);
    contentBox().append(m_apiKeyEntry);
    contentBox().append(m_showKeyCheck);

    addCancelButton();
    addOkButton(loc.translate("dialog.connect.button.connect"));

    m_apiUrlEntry.set_activates_default(true);
    m_apiKeyEntry.set_activates_default(true);

    updateValidation();
}

CloudStorageConfig ConnectDialog::currentConfig() const
{
    CloudStorageConfig config{ .apiUrl = m_apiUrlEntry.get_text(), .apiKey = m_apiKeyEntry.get_text() };
    config.apiUrl = config.normalizedApiUrl();
    return config;
}

void ConnectDialog::updateValidation()
{
    const auto& loc = LocaleManager::instance();
    const auto config = currentConfig();
    const auto urlStatus = config.urlStatus();

    m_apiUrlHintLabel.remove_css_class("error");
    m_apiUrlHintLabel.remove_css_class("warning");

    switch (urlStatus)
    {
        case ApiUrlStatus::Invalid:
            m_apiUrlHintLabel.set_text(loc.translate("dialog.connect.url.invalid"));
            m_apiUrlHintLabel.add_css_class("error");
            m_apiUrlHintLabel.set_visible(true);
            break;
        case ApiUrlStatus::Insecure:
            m_apiUrlHintLabel.set_text(loc.translate("dialog.connect.url.insecure"));
            m_apiUrlHintLabel.add_css_class("warning");
            m_apiUrlHintLabel.set_visible(true);
            break;
        case ApiUrlStatus::Empty:
        case ApiUrlStatus::Secure:
            m_apiUrlHintLabel.set_visible(false);
            break;
    }

    const bool urlUsable = urlStatus == ApiUrlStatus::Secure || urlStatus == ApiUrlStatus::Insecure;
    set_response_sensitive(Gtk::ResponseType::OK, urlUsable && !config.apiKey.empty());
}

std::optional<CloudStorageConfig> ConnectDialog::getResult() const
{
    const auto config = currentConfig();

    const auto urlStatus = config.urlStatus();
    const bool urlUsable = urlStatus == ApiUrlStatus::Secure || urlStatus == ApiUrlStatus::Insecure;

    if (!urlUsable || config.apiKey.empty())
        return std::nullopt;

    return config;
}

void ConnectDialog::prefillConfig(const CloudStorageConfig& config)
{
    m_apiUrlEntry.set_text(config.apiUrl);
    m_apiKeyEntry.set_text(config.apiKey);
}