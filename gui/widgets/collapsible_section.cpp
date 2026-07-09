// gui/widgets/collapsible_section.cpp

#include "collapsible_section.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

#include "panels/abstract_panel.h"

namespace gui {

CollapsibleSection::CollapsibleSection(const QString& title, QWidget* parent)
    : QWidget(parent), title_(title) {
    setAttribute(Qt::WA_StyledBackground, true);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // Header — a flat, left-aligned, bold button whose text prefix is the
    // expand/collapse arrow. The whole header is clickable.
    header_btn_ = new QPushButton(QStringLiteral("▼ ") + title_, this);
    header_btn_->setFixedHeight(28);
    header_btn_->setCursor(Qt::ArrowCursor);
    header_btn_->setFocusPolicy(Qt::NoFocus);
    header_btn_->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  text-align: left; padding: 2px 8px;"
        "  font-weight: bold;"
        "  background: transparent; border: none;"
        "  border-bottom: 1px solid rgba(128,128,128,60);"
        "}"
        "QPushButton:hover { background-color: rgba(128,128,128,40); }"));
    connect(header_btn_, &QPushButton::clicked, this, [this]() {
        set_collapsed(!collapsed_);
    });
    outer->addWidget(header_btn_);

    // Content area — panels/widgets are stacked here with spacing=8.
    content_ = new QWidget(this);
    content_layout_ = new QVBoxLayout(content_);
    content_layout_->setContentsMargins(8, 4, 8, 4);
    content_layout_->setSpacing(8);
    outer->addWidget(content_);
}

void CollapsibleSection::add_panel(AbstractPanel* panel) {
    if (!panel) return;
    auto* gb = new QGroupBox(panel->panel_title(), content_);
    auto* l = new QVBoxLayout(gb);
    l->setContentsMargins(6, 6, 6, 6);
    l->addWidget(panel);
    content_layout_->addWidget(gb);
}

void CollapsibleSection::add_widget(QWidget* w) {
    if (!w) return;
    content_layout_->addWidget(w);
}

void CollapsibleSection::set_collapsed(bool collapsed) {
    collapsed_ = collapsed;
    content_->setVisible(!collapsed);
    header_btn_->setText((collapsed ? QStringLiteral("▶ ") : QStringLiteral("▼ "))
                         + title_);
    // Persist so the state survives restarts (design §3.7.2).
    QSettings s;
    s.setValue(QStringLiteral("layout/section_%1_collapsed").arg(title_), collapsed);
    emit collapsed_changed(title_, collapsed);
}

} // namespace gui
