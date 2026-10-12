#include "ConnectDialog.hpp"

#include <utility>

#include "stapik/locale/LocaleManager.hpp"

using namespace stapik::cloud;

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

    m_passphraseLabel.set_text(loc.translate("dialog.connect.encryption.label"));
    m_passphraseLabel.set_halign(Gtk::Align::START);
    m_passphraseEntry.set_placeholder_text(loc.translate("dialog.connect.encryption.placeholder"));
    m_passphraseEntry.set_visibility(false);

    m_passphraseHintLabel.set_text(loc.translate("dialog.connect.encryption.hint"));
    m_passphraseHintLabel.set_halign(Gtk::Align::START);
    m_passphraseHintLabel.set_wrap(true);
    m_passphraseHintLabel.add_css_class("dim-label");

    m_apiUrlHintLabel.set_halign(Gtk::Align::START);
    m_apiUrlHintLabel.set_wrap(true);
    m_apiUrlHintLabel.set_visible(false);

    m_showKeyCheck.set_label(loc.translate("dialog.connect.key.show"));
    m_showKeyCheck.signal_toggled().connect([this]
    {
        m_apiKeyEntry.set_visibility(m_showKeyCheck.get_active());
        m_passphraseEntry.set_visibility(m_showKeyCheck.get_active());
    });

    m_checkButton.set_label(loc.translate("dialog.connect.button.check"));
    m_checkButton.set_halign(Gtk::Align::START);
    m_checkButton.signal_clicked().connect([this] { startConnectionCheck(); });

    m_checkResultLabel.set_halign(Gtk::Align::START);
    m_checkResultLabel.set_wrap(true);
    m_checkResultLabel.set_visible(false);

    m_apiUrlEntry.signal_changed().connect([this] { updateValidation(); });
    m_apiKeyEntry.signal_changed().connect([this] { updateValidation(); });
    m_passphraseEntry.signal_changed().connect([this] { clearCheckResult(); });

    contentBox().append(m_apiUrlLabel);
    contentBox().append(m_apiUrlEntry);
    contentBox().append(m_apiUrlHintLabel);
    contentBox().append(m_apiKeyLabel);
    contentBox().append(m_apiKeyEntry);
    contentBox().append(m_passphraseLabel);
    contentBox().append(m_passphraseEntry);
    contentBox().append(m_passphraseHintLabel);
    contentBox().append(m_showKeyCheck);
    contentBox().append(m_checkButton);
    contentBox().append(m_checkResultLabel);

    addCancelButton();
    addOkButton(loc.translate("dialog.connect.button.connect"));

    m_apiUrlEntry.set_activates_default(true);
    m_apiKeyEntry.set_activates_default(true);
    m_passphraseEntry.set_activates_default(true);

    updateValidation();
}

CloudStorageConfig ConnectDialog::currentConfig() const
{
    CloudStorageConfig config{
        .apiUrl = m_apiUrlEntry.get_text(),
        .apiKey = m_apiKeyEntry.get_text(),
        .encryptionPassphrase = m_passphraseEntry.get_text()
    };
    config.apiUrl = config.normalizedApiUrl();
    return config;
}

void ConnectDialog::updateValidation()
{
    using enum ApiUrlStatus;
    const auto& loc = LocaleManager::instance();
    const auto config = currentConfig();
    const auto urlStatus = config.urlStatus();

    m_apiUrlHintLabel.remove_css_class("error");
    m_apiUrlHintLabel.remove_css_class("warning");

    switch (urlStatus)
    {
        case Invalid:
            m_apiUrlHintLabel.set_text(loc.translate("dialog.connect.url.invalid"));
            m_apiUrlHintLabel.add_css_class("error");
            m_apiUrlHintLabel.set_visible(true);
            break;
        case Insecure:
            m_apiUrlHintLabel.set_text(loc.translate("dialog.connect.url.insecure"));
            m_apiUrlHintLabel.add_css_class("warning");
            m_apiUrlHintLabel.set_visible(true);
            break;
        case Empty:
        case Secure:
            m_apiUrlHintLabel.set_visible(false);
            break;
    }

    set_response_sensitive(Gtk::ResponseType::OK, isConfigUsable());
    clearCheckResult();
}

bool ConnectDialog::isConfigUsable() const
{
    const auto config = currentConfig();
    const auto urlStatus = config.urlStatus();
    const bool urlUsable = urlStatus == ApiUrlStatus::Secure || urlStatus == ApiUrlStatus::Insecure;
    return urlUsable && !config.apiKey.empty();
}

void ConnectDialog::clearCheckResult()
{
    ++m_checkGeneration;
    m_checkResultLabel.set_visible(false);
    m_checkButton.set_sensitive(isConfigUsable());
}

void ConnectDialog::startConnectionCheck()
{
    const auto& loc = LocaleManager::instance();

    m_checkButton.set_sensitive(false);
    m_checkResultLabel.remove_css_class("error");
    m_checkResultLabel.remove_css_class("success");
    m_checkResultLabel.set_text(loc.translate("dialog.connect.check.running"));
    m_checkResultLabel.set_visible(true);

    const auto generation = m_checkGeneration;
    m_checkRunner.submit(
        [config = currentConfig()] { return checkConnection(config); },
        [this, generation](std::optional<ConnectionCheckResult> const& result)
        {
            if (generation == m_checkGeneration)
                showCheckResult(result);
        });
}

void ConnectDialog::showCheckResult(const std::optional<ConnectionCheckResult>& result)
{
    const auto& loc = LocaleManager::instance();

    m_checkButton.set_sensitive(isConfigUsable());
    m_checkResultLabel.set_visible(true);

    if (result && result->hasValue())
    {
        m_checkResultLabel.set_text(loc.translate("dialog.connect.check.ok"));
        m_checkResultLabel.add_css_class("success");
        return;
    }

    m_checkResultLabel.add_css_class("error");

    if (!result)
    {
        m_checkResultLabel.set_text(loc.translate("dialog.connect.check.failed"));
        return;
    }

    switch (const auto&[kind, httpStatus, detail] = result->error(); kind)
    {
        using enum ConnectionErrorKind;
        case InvalidApiKey:
            m_checkResultLabel.set_text(loc.translate("dialog.connect.check.invalidKey"));
            break;
        case UnexpectedStatus:
            m_checkResultLabel.set_text(loc.translate("dialog.connect.check.status", { { "status", std::to_string(httpStatus) } }));
            break;
        case Unreachable:
            m_checkResultLabel.set_text(loc.translate("dialog.connect.check.unreachable", { { "error", detail } }));
            break;
    }
}

std::optional<CloudStorageConfig> ConnectDialog::getResult() const
{
    if (!isConfigUsable())
        return std::nullopt;

    return currentConfig();
}

void ConnectDialog::prefillConfig(const CloudStorageConfig& config)
{
    m_apiUrlEntry.set_text(config.apiUrl);
    m_apiKeyEntry.set_text(config.apiKey);
    m_passphraseEntry.set_text(config.encryptionPassphrase);
}

void showConnectDialog(
    Gtk::Window& parent,
    const std::optional<CloudStorageConfig>& prefill,
    std::function<void(const CloudStorageConfig&)> onConfirmed)
{
    auto* dialog = new ConnectDialog(parent);

    if (prefill)
        dialog->prefillConfig(*prefill);

    dialog->signal_response().connect([dialog, onConfirmed = std::move(onConfirmed)](const int response)
    {
        if (response == static_cast<int>(Gtk::ResponseType::OK) && onConfirmed)
        {
            if (const auto config = dialog->getResult())
                onConfirmed(*config);
        }

        dialog->hide();
    });

    dialog->signal_hide().connect([dialog] { delete dialog; });
    dialog->show();
}
