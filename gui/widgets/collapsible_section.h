// gui/widgets/collapsible_section.h — VSCode-style collapsible section.
//
// Design §3.7.2 (Phase 3 interaction enhancement). A section is a titled,
// collapsible container that stacks panels vertically. The sidebar
// (SettingsPanel) regresses from a two-tab layout to a stack of these
// sections, one per panel_group(), matching design.md §5.1's original
// vertical-stacked layout.
//
// Header: a flat QPushButton showing "▼ Title" (expanded) or "▶ Title"
// (collapsed); clicking toggles the content area's visibility. Collapse
// state is persisted to QSettings under "layout/section_<title>_collapsed"
// so it survives restarts.
//
// Content: a QVBoxLayout (spacing 8) holding one QGroupBox per added panel
// (the group box shows the panel's panel_title()), plus any plain widgets
// added via add_widget() (used for the calibration placeholder).

#ifndef GUI_WIDGETS_COLLAPSIBLE_SECTION_H
#define GUI_WIDGETS_COLLAPSIBLE_SECTION_H

#include <QString>
#include <QWidget>

class QGroupBox;
class QPushButton;
class QVBoxLayout;

namespace gui {

class AbstractPanel;

class CollapsibleSection : public QWidget {
    Q_OBJECT
public:
    explicit CollapsibleSection(const QString& title, QWidget* parent = nullptr);

    /// Adds a panel below the title header, wrapped in a titled QGroupBox
    /// showing the panel's panel_title().
    void add_panel(AbstractPanel* panel);

    /// Adds an arbitrary widget below the title header (e.g. the calibration
    /// placeholder group box).
    void add_widget(QWidget* w);

    void set_collapsed(bool collapsed);
    bool is_collapsed() const { return collapsed_; }
    QString title() const { return title_; }

signals:
    void collapsed_changed(const QString& title, bool collapsed);

private:
    QString title_;
    bool collapsed_{false};
    QPushButton* header_btn_{nullptr};
    QWidget* content_{nullptr};
    QVBoxLayout* content_layout_{nullptr};
};

} // namespace gui

#endif // GUI_WIDGETS_COLLAPSIBLE_SECTION_H
