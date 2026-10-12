#pragma once

#include "StapikDialog.hpp"

#include "stapik/cloud/CloudStorageConfig.hpp"
#include "stapik/cloud/ConnectionCheck.hpp"
#include "stapik/task/BackgroundTaskRunner.hpp"

#include <gtkmm/button.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/entry.h>
#include <gtkmm/label.h>

#include <functional>
#include <optional>

class ConnectDialog : public StapikDialog
{
public:
    explicit ConnectDialog(Window& parent);
    [[nodiscard]] std::optional<CloudStorageConfig> getResult() const;
    void prefillConfig(const CloudStorageConfig& config);
private:
    Gtk::Label m_apiUrlLabel;
    Gtk::Entry m_apiUrlEntry;
    Gtk::Label m_apiUrlHintLabel;
    Gtk::Label m_apiKeyLabel;
    Gtk::Entry m_apiKeyEntry;
    Gtk::Label m_passphraseLabel;
    Gtk::Entry m_passphraseEntry;
    Gtk::Label m_passphraseHintLabel;
    Gtk::CheckButton m_showKeyCheck;
    Gtk::Button m_checkButton;
    Gtk::Label m_checkResultLabel;
    stapik::task::BackgroundTaskRunner m_checkRunner;
    unsigned m_checkGeneration = 0;

    void initLayout();
    [[nodiscard]] CloudStorageConfig currentConfig() const;
    void updateValidation();
    void startConnectionCheck();
    void showCheckResult(const std::optional<stapik::cloud::ConnectionCheckResult>& result);
    void clearCheckResult();
    [[nodiscard]] bool isConfigUsable() const;
};

void showConnectDialog(
    Gtk::Window& parent,
    const std::optional<CloudStorageConfig>& prefill,
    std::function<void(const CloudStorageConfig&)> onConfirmed);
