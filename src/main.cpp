#include "ui/MainWindow.h"

#include <QApplication>

// Qt GUI entry: lobby, then a GameSession (practice / native / classic 1.13).
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Tetrinet"));
    app.setOrganizationName(QStringLiteral("tetrinet"));

    tnet::MainWindow w;
    w.show();
    return app.exec();
}
