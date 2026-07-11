#pragma once

#include "stapik/cloud/CloudStorageConfig.hpp"

#include <gtkmm/dialog.h>
#include <gtkmm/entry.h>
#include <gtkmm/label.h>
#include <gtkmm/box.h>

#include <optional>

class ConnectDialog : public Gtk::Dialog
{
public:
    explicit ConnectDialog(Window& parent);
    [[nodiscard]] std::optional<CloudStorageConfig> getResult() const;
    void prefillConfig(const CloudStorageConfig& config);
private:
    static constexpr int CONTENT_SPACING = 8;
    static constexpr int CONTENT_MARGIN = 16;
    static constexpr int DEFAULT_WIDTH = 400;

    Gtk::Box m_contentBox;
    Gtk::Label m_apiUrlLabel;
    Gtk::Entry m_apiUrlEntry;
    Gtk::Label m_apiKeyLabel;
    Gtk::Entry m_apiKeyEntry;

    void initLayout();
};