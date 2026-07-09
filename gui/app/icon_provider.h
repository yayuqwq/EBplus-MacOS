// gui/app/icon_provider.h — recolorable SVG icon provider (Lucide-based).
//
// Icons are loaded from the Qt resource system at ":/icons/<name>.svg" and
// recolored by replacing the SVG's "currentColor" with the requested color,
// then rendered via QSvgRenderer. This gives cross-platform consistent icons
// that follow the active theme's foreground color.

#ifndef GUI_APP_ICON_PROVIDER_H
#define GUI_APP_ICON_PROVIDER_H

#include <QColor>
#include <QIcon>
#include <QString>

namespace gui {

class IconProvider {
public:
    /// Returns @p name recolored with the active theme's primary foreground
    /// color (read from the application palette so it always tracks the
    /// current ThemeController selection).
    static QIcon get(const QString& name);

    /// Returns @p name recolored with the explicit @p color. Use this for
    /// status indicators (green / red / gray dots, etc.).
    static QIcon get(const QString& name, const QColor& color);

private:
    /// Renders the SVG named @p name with @p color into a QIcon. Returns a
    /// null icon if the resource is missing or invalid.
    static QIcon render(const QString& name, const QColor& color);
};

} // namespace gui

#endif // GUI_APP_ICON_PROVIDER_H
