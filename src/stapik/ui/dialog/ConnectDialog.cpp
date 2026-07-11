#include "ConnectDialog.hpp"

#include "stapik/locale/LocaleManager.hpp"

ConnectDialog::ConnectDialog(Window& parent) :
    Dialog(LocaleManager::instance().translate("dialog.connect.title"), parent, true),
    m_contentBox(Gtk::Orientation::VERTICAL, CONTENT_SPACING)
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

    m_contentBox.set_margin(CONTENT_MARGIN);
    m_contentBox.append(m_apiUrlLabel);
    m_contentBox.append(m_apiUrlEntry);
    m_contentBox.append(m_apiKeyLabel);
    m_contentBox.append(m_apiKeyEntry);

    get_content_area()->append(m_contentBox);

    add_button(loc.translate("dialog.button.cancel"), Gtk::ResponseType::CANCEL);
    add_button(loc.translate("dialog.connect.button.connect"), Gtk::ResponseType::OK);

    set_default_response(Gtk::ResponseType::OK);
    m_apiUrlEntry.set_activates_default(true);
    m_apiKeyEntry.set_activates_default(true);
    set_default_size(DEFAULT_WIDTH, -1);
}

std::optional<CloudStorageConfig> ConnectDialog::getResult() const
{
    const auto apiUrl = m_apiUrlEntry.get_text();
    const auto apiKey = m_apiKeyEntry.get_text();

    if (apiUrl.empty() || apiKey.empty())
        return std::nullopt;

    return CloudStorageConfig{ apiUrl, apiKey };
}

void ConnectDialog::prefillConfig(const CloudStorageConfig& config)
{
    m_apiUrlEntry.set_text(config.apiUrl);
    m_apiKeyEntry.set_text(config.apiKey);
}