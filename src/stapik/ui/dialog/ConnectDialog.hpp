#pragma once

#include "StapikDialog.hpp"

#include "stapik/cloud/CloudStorageConfig.hpp"

#include <gtkmm/checkbutton.h>
#include <gtkmm/entry.h>
#include <gtkmm/label.h>
#include <gtkmm/box.h>

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
    Gtk::CheckButton m_showKeyCheck;

    void initLayout();
    [[nodiscard]] CloudStorageConfig currentConfig() const;
    void updateValidation();
};