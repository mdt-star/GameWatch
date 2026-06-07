#include <QApplication>
#include <QIcon>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("GameWatch");
    app.setOrganizationName("GameWatch");
    app.setApplicationVersion("1.0.0");

    MainWindow w;
    w.show();

    return app.exec();
}