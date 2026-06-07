#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
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
    void setupUI();
    void loadSettings();
    void saveSettings();
    void sendAlert(int currentCount);    // 只在满足条件时发送推送

    // Configuration controls
    QLineEdit   *m_windowMatchEdit;
    QSpinBox    *m_thresholdSpin;
    QLineEdit   *m_pushKeyEdit;
    QLineEdit   *m_pushUrlEdit;
    QTextEdit   *m_messageTemplateEdit;
    QCheckBox   *m_enabledCheck;
    QSpinBox    *m_cooldownSpin;       // 冷却时间（秒）

    // Status
    QLabel      *m_statusLabel;
    QLabel      *m_countLabel;
    QPushButton *m_startBtn;
    QPushButton *m_testPushBtn;
    QPushButton *m_applyBtn;

    // Core
    WindowMonitor   *m_monitor;
    PushDeerClient  *m_pushClient;
    QDateTime        m_lastAlertTime;  // 上次推送时间，用于冷却
    int              m_lastCount;      // 缓存最近一次窗口数，给 UI 定时器用
    QTimer          *m_uiTimer;        // 1秒定时器，统一刷新状态栏
    bool             m_isWaitingPush;  // 标记是否需要发送推送（冷却结束后为 true）
};

#endif // MAINWINDOW_H