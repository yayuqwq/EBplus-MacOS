// gui/panels/settings_panel.cpp

#include "settings_panel.h"

#include <QGroupBox>
#include <QLabel>
#include <QScrollArea>
#include <QSettings>
#include <QVBoxLayout>
#include <utility>

#include "algorithms_panel.h"
#include "biases_panel.h"
#include "devices_panel.h"
#include "display_panel.h"
#include "esp_panel.h"
#include "file_tools_panel.h"
#include "information_panel.h"
#include "preprocessing_panel.h"
#include "roi_panel.h"
#include "statistics_panel.h"
#include "trigger_panel.h"
#include "algo_bridge/algo_bridge.h"
#include "app/file_converter.h"
#include "widgets/collapsible_section.h"

namespace gui {

AbstractPanel* SettingsPanel::register_panel(std::unique_ptr<AbstractPanel> panel) {
    AbstractPanel* raw = panel.get();
    panel_index_[raw->panel_id()] = raw;
    panels_.push_back(std::move(panel));
    return raw;
}

AbstractPanel* SettingsPanel::find_panel(const QString& id) const {
    auto it = panel_index_.find(id);
    return it == panel_index_.end() ? nullptr : it->second;
}

std::vector<AbstractPanel*> SettingsPanel::panels_in_group(const QString& group) const {
    std::vector<AbstractPanel*> out;
    for (const auto& p : panels_) {
        if (p->panel_group() == group) out.push_back(p.get());
    }
    return out;
}

SettingsPanel::SettingsPanel(AlgoBridge* bridge, FileConverter* converter,
                             QWidget* parent)
    : QWidget(parent) {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    // --- Phase 3 (§3.7): VSCode-style stacked CollapsibleSections. ---
    // Panels are aggregated by panel_group(); each group becomes one
    // collapsible section. The whole stack lives in a scroll area so all
    // sections remain reachable even when they exceed the viewport.
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* host = new QWidget(scroll);
    auto* host_layout = new QVBoxLayout(host);
    host_layout->setContentsMargins(6, 6, 6, 6);
    host_layout->setSpacing(8);

    // Register every panel into the registry first (ownership via panels_).
    // The unique_ptr is the authoritative owner; add_panel reparents each
    // widget into a group box inside its section (Qt removes it from the old
    // parent first), which is safe — deleting a QWidget removes it from its
    // parent's child list, so the unique_ptr can destroy it later.
    register_panel(std::make_unique<DevicesPanel>(nullptr));
    register_panel(std::make_unique<InformationPanel>(nullptr));
    register_panel(std::make_unique<StatisticsPanel>(nullptr));
    register_panel(std::make_unique<DisplayPanel>(nullptr));
    register_panel(std::make_unique<BiasesPanel>(nullptr));
    register_panel(std::make_unique<RoiPanel>(nullptr));
    register_panel(std::make_unique<EspPanel>(nullptr));
    register_panel(std::make_unique<TriggerPanel>(nullptr));
    register_panel(std::make_unique<PreprocessingPanel>(nullptr));
    register_panel(std::make_unique<FileToolsPanel>(converter, nullptr));
    register_panel(std::make_unique<AlgorithmsPanel>(bridge, nullptr));

    // Group order + default collapse state (design §3.7.3 table).
    struct GroupDef { QString name; bool default_collapsed; };
    const GroupDef groups[] = {
        {QStringLiteral("相机设备"),   false},
        {QStringLiteral("显示与统计"), false},
        {QStringLiteral("硬件配置"),   false},
        {QStringLiteral("算法模块"),   true},
        {QStringLiteral("工具"),       true},
    };

    QSettings s;
    for (const auto& g : groups) {
        const auto panels = panels_in_group(g.name);
        if (panels.empty()) continue;

        auto* section = new CollapsibleSection(g.name, host);
        for (auto* p : panels) {
            section->add_panel(p);
        }

        // The 工具 group also holds the Calibration placeholder (Phase 9
        // install target for set_calibration_panel). The wizard itself is
        // launched from the Tools menu; this group box just shows a hint.
        if (g.name == QStringLiteral("工具")) {
            calibration_group_ = new QGroupBox(tr("Calibration"), section);
            auto* gl = new QVBoxLayout(calibration_group_);
            gl->setContentsMargins(6, 6, 6, 6);
            auto* lbl = new QLabel(
                tr("Open via menu: Tools → Intrinsic Wizard..."),
                calibration_group_);
            lbl->setWordWrap(true);
            lbl->setProperty("class", "hint");
            gl->addWidget(lbl);
            calibration_group_->setEnabled(false);
            section->add_widget(calibration_group_);
        }

        // Apply persisted (or default) collapse state. set_collapsed also
        // writes the value back to QSettings, which is harmless.
        const QString key =
            QStringLiteral("layout/section_%1_collapsed").arg(g.name);
        const bool collapsed = s.value(key, g.default_collapsed).toBool();
        section->set_collapsed(collapsed);

        host_layout->addWidget(section);
    }

    host_layout->addStretch(1);
    scroll->setWidget(host);
    outer->addWidget(scroll);
}

// Type-safe accessors — each delegates to find_panel() + static_cast so the
// registry (panels_ / panel_index_) is the single source of truth.
InformationPanel*   SettingsPanel::information_panel()    const { return static_cast<InformationPanel*>(find_panel(QStringLiteral("information"))); }
StatisticsPanel*    SettingsPanel::statistics_panel()     const { return static_cast<StatisticsPanel*>(find_panel(QStringLiteral("statistics"))); }
DisplayPanel*       SettingsPanel::display_panel()        const { return static_cast<DisplayPanel*>(find_panel(QStringLiteral("display"))); }
DevicesPanel*       SettingsPanel::devices_panel()        const { return static_cast<DevicesPanel*>(find_panel(QStringLiteral("devices"))); }
BiasesPanel*        SettingsPanel::biases_panel()         const { return static_cast<BiasesPanel*>(find_panel(QStringLiteral("biases"))); }
RoiPanel*           SettingsPanel::roi_panel()            const { return static_cast<RoiPanel*>(find_panel(QStringLiteral("roi"))); }
EspPanel*           SettingsPanel::esp_panel()            const { return static_cast<EspPanel*>(find_panel(QStringLiteral("esp"))); }
TriggerPanel*       SettingsPanel::trigger_panel()        const { return static_cast<TriggerPanel*>(find_panel(QStringLiteral("trigger"))); }
PreprocessingPanel* SettingsPanel::preprocessing_panel() const { return static_cast<PreprocessingPanel*>(find_panel(QStringLiteral("preprocessing"))); }
AlgorithmsPanel*    SettingsPanel::algorithms_panel()     const { return static_cast<AlgorithmsPanel*>(find_panel(QStringLiteral("algorithms"))); }
FileToolsPanel*     SettingsPanel::file_tools_panel()     const { return static_cast<FileToolsPanel*>(find_panel(QStringLiteral("file_tools"))); }

void SettingsPanel::set_calibration_panel(QWidget* panel) {
    if (!panel || !calibration_group_) return;
    if (panel == calibration_installed_) return;
    // Remove the previously-installed panel if any; otherwise remove the
    // placeholder label. Using findChildren with FindDirectChildrenOnly
    // avoids recursively deleting QLabels inside an already-installed panel
    // (which the previous implementation did, corrupting the UI on the
    // second call).
    if (calibration_installed_) {
        calibration_installed_->deleteLater();
        calibration_installed_ = nullptr;
    } else {
        const auto old_lbls = calibration_group_->findChildren<QLabel*>(
            QString(), Qt::FindDirectChildrenOnly);
        for (auto* l : old_lbls) l->deleteLater();
    }
    auto* gl = qobject_cast<QVBoxLayout*>(calibration_group_->layout());
    if (gl) {
        gl->addWidget(panel);
    } else {
        auto* ngl = new QVBoxLayout(calibration_group_);
        ngl->setContentsMargins(6, 6, 6, 6);
        ngl->addWidget(panel);
    }
    calibration_group_->setEnabled(true);
    calibration_installed_ = panel;
}

} // namespace gui
