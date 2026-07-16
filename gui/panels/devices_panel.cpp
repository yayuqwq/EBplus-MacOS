// gui/panels/devices_panel.cpp

#include "devices_panel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace gui {

DevicesPanel::DevicesPanel(QWidget* parent) : AbstractPanel(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);

    list_ = new QListWidget(this);
    list_->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(list_);

    auto* row1 = new QHBoxLayout();
    btn_refresh_ = new QPushButton(tr("Refresh"), this);
    btn_connect_first_ = new QPushButton(tr("Connect first"), this);
    row1->addWidget(btn_refresh_);
    row1->addWidget(btn_connect_first_);
    layout->addLayout(row1);

    auto* row2 = new QHBoxLayout();
    btn_connect_selected_ = new QPushButton(tr("Connect selected"), this);
    btn_disconnect_ = new QPushButton(tr("Disconnect"), this);
    row2->addWidget(btn_connect_selected_);
    row2->addWidget(btn_disconnect_);
    layout->addLayout(row2);

    btn_self_test_ = new QPushButton(tr("Sensor Self-Test"), this);
    btn_self_test_->setToolTip(
        tr("<b>Sensor Self-Test</b><br><br>"
           "Detects bad pixels and estimates the per-pixel refractory period "
           "(minimum inter-event interval).<br><br>"
           "<b>How to use:</b><br>"
           "1. Click to open the heatmap window.<br>"
           "2. Shake the camera to stimulate event generation across the "
           "entire sensor.<br>"
           "3. Close the window when done — a report dialog will appear "
           "with statistics and suspected bad-pixel coordinates.<br><br>"
           "<b>Image meaning:</b><br>"
           "• <span style='color:red'>Red</span> — pixel never triggered "
           "(suspected bad pixel).<br>"
           "• Gray — refractory period (brighter = shorter, mapped "
           "exponentially from 1us to 10000us).<br>"
           "• Black — only one event so far (insufficient data)."));
    layout->addWidget(btn_self_test_);

    set_connected(false);

    connect(btn_refresh_, &QPushButton::clicked, this, &DevicesPanel::refresh_requested);
    connect(btn_connect_first_, &QPushButton::clicked, this, &DevicesPanel::connect_first_requested);
    connect(btn_connect_selected_, &QPushButton::clicked, this, [this]() {
        auto* item = list_->currentItem();
        if (!item) {
            return;
        }
        const QString serial = item->data(Qt::UserRole).toString();
        if (!serial.isEmpty()) {
            emit connect_serial_requested(serial);
        }
    });
    connect(btn_disconnect_, &QPushButton::clicked, this, &DevicesPanel::disconnect_requested);
    connect(btn_self_test_, &QPushButton::clicked, this, &DevicesPanel::self_test_requested);
}

void DevicesPanel::refresh_sources(const std::vector<std::pair<QString, QString>>& sources) {
    list_->clear();
    for (const auto& src : sources) {
        auto* item = new QListWidgetItem(
            QStringLiteral("%1 — %2").arg(src.first, src.second), list_);
        item->setData(Qt::UserRole, src.second);
    }
    if (list_->count() == 0) {
        new QListWidgetItem(tr("No cameras detected"), list_);
    }
}

void DevicesPanel::set_connected(bool connected) {
    btn_connect_first_->setEnabled(!connected);
    btn_connect_selected_->setEnabled(!connected);
    btn_disconnect_->setEnabled(connected);
    btn_self_test_->setEnabled(connected);
}

} // namespace gui
