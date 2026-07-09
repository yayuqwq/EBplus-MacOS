// gui/panels/settings_panel.h — right-dock container.
//
// Phase 3 (§3.7): the sidebar regresses from the two-tab layout to a VSCode-
// style stack of CollapsibleSection widgets, one per panel_group(). Panels
// are aggregated via panels_in_group(); each section's collapse state is
// persisted by CollapsibleSection itself. The dock can be hidden via the
// View menu or Ctrl+Shift+S to maximize the display area.
//
// Phase 2 (§3.3.3): panels are stored in a registry (panels_) keyed by
// panel_id(), so MainWindow looks up panels via find_panel("biases") instead
// of a hardcoded accessor per panel. The registry is the single source of
// truth; the visible layout (now stacked sections) is independent of it, so
// programmatic find_panel() access is unaffected by the layout change.

#ifndef GUI_PANELS_SETTINGS_PANEL_H
#define GUI_PANELS_SETTINGS_PANEL_H

#include <QString>
#include <QWidget>
#include <memory>
#include <unordered_map>
#include <vector>

#include "abstract_panel.h"

class QGroupBox;
class QTabWidget;

namespace gui {

class InformationPanel;
class StatisticsPanel;
class DisplayPanel;
class DevicesPanel;
class BiasesPanel;
class RoiPanel;
class EspPanel;
class TriggerPanel;
class PreprocessingPanel;
class AlgorithmsPanel;
class FileToolsPanel;
class AlgoBridge;
class FileConverter;

class SettingsPanel : public QWidget {
    Q_OBJECT
public:
    explicit SettingsPanel(AlgoBridge* bridge = nullptr,
                           FileConverter* converter = nullptr,
                           QWidget* parent = nullptr);

    /// @brief Registers a panel into the internal registry. The panel is
    /// keyed by its panel_id(); lookups use find_panel(). Ownership of the
    /// unique_ptr transfers to SettingsPanel.
    /// @return Raw pointer to the registered panel (still valid until this
    /// SettingsPanel is destroyed).
    AbstractPanel* register_panel(std::unique_ptr<AbstractPanel> panel);

    /// @brief Looks up a panel by id. Returns nullptr if not found.
    AbstractPanel* find_panel(const QString& id) const;

    /// @brief Returns all panels whose panel_group() equals @p group.
    std::vector<AbstractPanel*> panels_in_group(const QString& group) const;

    /// @brief Returns all registered panels.
    const std::vector<std::unique_ptr<AbstractPanel>>& panels() const { return panels_; }

    // Type-safe accessors (kept for MainWindow compatibility; each delegates
    // to find_panel() + static_cast so the registry is the single source of
    // truth). Phase 3 will migrate MainWindow to find_panel() directly.
    InformationPanel*   information_panel()    const;
    StatisticsPanel*    statistics_panel()     const;
    DisplayPanel*       display_panel()        const;
    DevicesPanel*       devices_panel()        const;
    BiasesPanel*        biases_panel()         const;
    RoiPanel*           roi_panel()            const;
    EspPanel*           esp_panel()            const;
    TriggerPanel*       trigger_panel()        const;
    PreprocessingPanel* preprocessing_panel() const;
    AlgorithmsPanel*    algorithms_panel()     const;
    FileToolsPanel*     file_tools_panel()     const;
    QTabWidget*         tab_widget()           const { return tabs_; }

    /// @brief Installs an externally-built calibration panel (Phase 9).
    void set_calibration_panel(QWidget* panel);

private:
    // Registry of all panels (owns via unique_ptr; Qt parent-child also
    // applies once the panel is added to a layout, but the unique_ptr is the
    // authoritative owner — see register_panel implementation note).
    std::vector<std::unique_ptr<AbstractPanel>> panels_;
    std::unordered_map<QString, AbstractPanel*> panel_index_;

    QTabWidget* tabs_{nullptr};

    // Phase 9 — calibration placeholder group box and any panel installed
    // into it. Tracked by pointer so set_calibration_panel can replace the
    // placeholder cleanly without recursive findChildren() searches that
    // would delete QLabels inside an already-installed panel.
    QGroupBox* calibration_group_{nullptr};
    QWidget* calibration_installed_{nullptr};
};

} // namespace gui

#endif // GUI_PANELS_SETTINGS_PANEL_H
