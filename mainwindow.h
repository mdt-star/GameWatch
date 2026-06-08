#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTabWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QSettings>
#include <QDateTime>
#include <QTimer>

class WindowMonitor;
class PushDeerClient;
class DD373Dialog;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onStartStop();
    void onWindowCountChanged(int count);
    void onTestPush();
    void onPushResult(bool success, const QString &error);
    void onApplySettings();
    void onUiTick();

private:
    void setupGameWatchTab(QWidget *tab);
    void setupDD373Tab(QWidget *tab);
    void loadSettings();
    void saveSettings();
    void sendAlert(int currentCount);

    QTabWidget *m_tabWidget;
    DD373Dialog *m_dd373Dialog;

    // GameWatch tab controls
    QLineEdit   *m_windowMatchEdit;
    QSpinBox    *m_thresholdSpin;
    QLineEdit   *m_pushKeyEdit;
    QLineEdit   *m_pushUrlEdit;
    QTextEdit   *m_messageTemplateEdit;
    QCheckBox   *m_enabledCheck;
    QSpinBox    *m_cooldownSpin;

    // Status
    QLabel      *m_statusLabel;
    QLabel      *m_countLabel;
    QPushButton *m_startBtn;
    QPushButton *m_testPushBtn;
    QPushButton *m_applyBtn;

    // Core
    WindowMonitor   *m_monitor;
    PushDeerClient  *m_pushClient;
    QDateTime        m_lastAlertTime;
    int              m_lastCount;
    QTimer          *m_uiTimer;
};

#endif // MAINWINDOW_H