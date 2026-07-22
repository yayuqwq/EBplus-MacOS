// gui/main.cpp — application entry point.

#include <QApplication>
#include <QFont>
#include <QSurfaceFormat>

#include "app/startup_environment.h"
#include "main_window.h"

int main(int argc, char* argv[]) {
    gui::startup_environment::apply_defaults_for_current_platform();

    // Request a core-profile OpenGL 3.3 context for the display widget.
    QSurfaceFormat fmt;
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setSwapInterval(1); // vsync on
    QSurfaceFormat::setDefaultFormat(fmt);

    QApplication app(argc, argv);
    QApplication::setApplicationName("GUI for openEB");
    QApplication::setOrganizationName("GUI-for-openEB");
    QApplication::setApplicationVersion("1.9.0");

    // Global UI font — Inter (design §3.9.1) with platform fallbacks so the
    // typeface stays consistent on systems without Inter installed.
    QFont font(QStringLiteral("Inter"), 10);
    font.setFamilies({QStringLiteral("Inter"),
                      QStringLiteral("Segoe UI"),
                      QStringLiteral("Ubuntu"),
                      QStringLiteral("Noto Sans"),
                      QStringLiteral("Sans Serif")});
    font.setStyleStrategy(QFont::PreferAntialias);
    app.setFont(font);

    gui::MainWindow window;
    window.show();

    return app.exec();
}
