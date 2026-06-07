#include <QApplication>
#include <QIcon>
#include <QSettings>
#include <QDir>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("GameWatch");
    app.setOrganizationName("GameWatch");
    app.setApplicationVersion("1.0.0");

    // 使用 INI 格式，配置文件保存在 exe 同级目录
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QString iniPath = QDir(QCoreApplication::applicationDirPath())
                          .filePath("GameWatch.ini");
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, iniPath);
    // 强制加载使路径生效
    QSettings settings;
    Q_UNUSED(settings);

    MainWindow w;
    w.show();

    return app.exec();
}