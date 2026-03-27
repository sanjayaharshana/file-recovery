#include "app_style.hpp"
#include "mainwindow.h"

#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("File Recovery"));
    QApplication::setOrganizationName(QStringLiteral("FileRecovery"));

    applyModernAppStyle(&app);

    MainWindow w;
    w.show();
    return app.exec();
}
