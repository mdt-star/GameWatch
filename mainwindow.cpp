#include "mainwindow.h"
#include "windowmonitor.h"
#include "pushdeerclient.h"
#include "dd373/dd373dialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QMessageBox>
#include <QDateTime>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_monitor(new WindowMonitor(this))
    , m_pushClient(new PushDeerClient(this))
    , m_lastCount(-1)
    , m_uiTimer(new QTimer(this))
{
    setWindowTitle(QStringLiteral("GameWatch - 游戏窗口监控"));
    setMinimumSize(500, 630);
    resize(520, 680);

    // Tab widget
    m_tabWidget = new QTabWidget(this);
    setCentralWidget(m_tabWidget);

    // Tab 1: GameWatch
    QWidget *gwTab = new QWidget(this);
    setupGameWatchTab(gwTab);
    m_tabWidget->addTab(gwTab, QStringLiteral("窗口监控"));

    // Tab 2: DD373
    m_dd373Dialog = new DD373Dialog(this);
    m_tabWidget->addTab(m_dd373Dialog, QStringLiteral("DD373 商品"));

    // Connect monitor signals
    connect(m_monitor, &WindowMonitor::windowCountChanged,
            this, &MainWindow::onWindowCountChanged);

    // Connect push client
    connect(m_pushClient, &PushDeerClient::messageSent,
            this, &MainWindow::onPushResult);

    // 1秒定时器，统一刷新状态栏
    connect(m_uiTimer, &QTimer::timeout, this, &MainWindow::onUiTick);
    m_uiTimer->start(1000);

    loadSettings();
}

MainWindow::~MainWindow() = default;

void MainWindow::setupGameWatchTab(QWidget *tab)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(tab);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(16, 16, 16, 16);

    // ---- Window Match Config ----
    QGroupBox *matchGroup = new QGroupBox(QStringLiteral("窗口匹配配置"), tab);
    QFormLayout *matchForm = new QFormLayout(matchGroup);

    m_windowMatchEdit = new QLineEdit(tab);
    m_windowMatchEdit->setPlaceholderText(QStringLiteral("例如: 原神 或 Genshin|Honkai"));
    m_windowMatchEdit->setToolTip(QStringLiteral("匹配窗口标题的关键词，支持正则表达式"));
    matchForm->addRow(QStringLiteral("窗口标题关键词:"), m_windowMatchEdit);

    m_thresholdSpin = new QSpinBox(tab);
    m_thresholdSpin->setRange(1, 999);
    m_thresholdSpin->setValue(1);
    m_thresholdSpin->setSuffix(QStringLiteral(" 个"));
    matchForm->addRow(QStringLiteral("低于此数量时报警:"), m_thresholdSpin);

    mainLayout->addWidget(matchGroup);

    // ---- PushDeer Config ----
    QGroupBox *pushGroup = new QGroupBox(QStringLiteral("PushDeer 推送配置"), tab);
    QFormLayout *pushForm = new QFormLayout(pushGroup);

    m_pushKeyEdit = new QLineEdit(tab);
    m_pushKeyEdit->setPlaceholderText(QStringLiteral("PDU... 或 PushDeer Key"));
    m_pushKeyEdit->setEchoMode(QLineEdit::Password);
    pushForm->addRow(QStringLiteral("PushDeer Key:"), m_pushKeyEdit);

    m_pushUrlEdit = new QLineEdit(tab);
    m_pushUrlEdit->setPlaceholderText(QStringLiteral("https://api2.pushdeer.com/message/push"));
    m_pushUrlEdit->setText(QStringLiteral("https://api2.pushdeer.com/message/push"));
    pushForm->addRow(QStringLiteral("推送服务地址:"), m_pushUrlEdit);

    mainLayout->addWidget(pushGroup);

    // ---- Message Template ----
    QGroupBox *msgGroup = new QGroupBox(QStringLiteral("推送消息内容"), tab);
    QVBoxLayout *msgLayout = new QVBoxLayout(msgGroup);

    m_messageTemplateEdit = new QTextEdit(tab);
    m_messageTemplateEdit->setPlaceholderText(
        QStringLiteral("可用变量:\n")
        + QStringLiteral("{count} - 当前匹配窗口数\n")
        + QStringLiteral("{threshold} - 设置的阈值\n")
        + QStringLiteral("{time} - 当前时间\n")
        + QStringLiteral("{match} - 匹配关键词\n\n")
        + QStringLiteral("示例: 游戏窗口不足! 当前仅 {count} 个, 低于阈值 {threshold} 个."));
    m_messageTemplateEdit->setMaximumHeight(120);
    msgLayout->addWidget(m_messageTemplateEdit);

    // ---- Cooldown ----
    QHBoxLayout *cdLayout = new QHBoxLayout();
    cdLayout->addWidget(new QLabel(QStringLiteral("推送冷却时间:"), tab));
    m_cooldownSpin = new QSpinBox(tab);
    m_cooldownSpin->setRange(10, 3600);
    m_cooldownSpin->setValue(60);
    m_cooldownSpin->setSuffix(QStringLiteral(" 秒"));
    m_cooldownSpin->setToolTip(QStringLiteral("两次推送之间的最短间隔时间，避免重复推送"));
    cdLayout->addWidget(m_cooldownSpin);
    cdLayout->addStretch();
    msgLayout->addLayout(cdLayout);

    mainLayout->addWidget(msgGroup);

    // ---- Status Area ----
    QGroupBox *statusGroup = new QGroupBox(QStringLiteral("运行状态"), tab);
    QVBoxLayout *statusLayout = new QVBoxLayout(statusGroup);

    m_countLabel = new QLabel(QStringLiteral("当前匹配窗口数: --"), tab);
    m_countLabel->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: bold;"));
    statusLayout->addWidget(m_countLabel);

    m_statusLabel = new QLabel(QStringLiteral("状态: 未启动"), tab);
    statusLayout->addWidget(m_statusLabel);

    mainLayout->addWidget(statusGroup);

    // ---- Control Buttons ----
    QHBoxLayout *btnLayout = new QHBoxLayout();

    m_enabledCheck = new QCheckBox(QStringLiteral("启动时自动监控"), tab);
    btnLayout->addWidget(m_enabledCheck);

    btnLayout->addStretch();

    m_testPushBtn = new QPushButton(QStringLiteral("测试推送"), tab);
    btnLayout->addWidget(m_testPushBtn);

    m_startBtn = new QPushButton(QStringLiteral("开始监控"), tab);
    m_startBtn->setMinimumHeight(36);
    btnLayout->addWidget(m_startBtn);

    m_applyBtn = new QPushButton(QStringLiteral("保存设置"), tab);
    m_applyBtn->setMinimumHeight(36);
    btnLayout->addWidget(m_applyBtn);

    mainLayout->addLayout(btnLayout);

    // Connect buttons
    connect(m_startBtn, &QPushButton::clicked, this, &MainWindow::onStartStop);
    connect(m_testPushBtn, &QPushButton::clicked, this, &MainWindow::onTestPush);
    connect(m_applyBtn, &QPushButton::clicked, this, &MainWindow::onApplySettings);
}

void MainWindow::setupDD373Tab(QWidget * /*tab*/)
{
    // DD373Dialog is set as a tab directly in the constructor
}

void MainWindow::loadSettings()
{
    QSettings settings;
    m_windowMatchEdit->setText(
        settings.value(QStringLiteral("window/match"), QString()).toString());
    m_thresholdSpin->setValue(
        settings.value(QStringLiteral("window/threshold"), 1).toInt());
    m_pushKeyEdit->setText(
        settings.value(QStringLiteral("pushdeer/key"), QString()).toString());
    m_pushUrlEdit->setText(
        settings.value(QStringLiteral("pushdeer/url"),
                       QStringLiteral("https://api2.pushdeer.com/message/push")).toString());
    m_messageTemplateEdit->setPlainText(
        settings.value(QStringLiteral("pushdeer/message_template"),
                       QStringLiteral("⚠️ 游戏窗口不足！当前仅 {count} 个窗口匹配 \"{match}\"，低于阈值 {threshold} 个。"))
            .toString());
    m_cooldownSpin->setValue(
        settings.value(QStringLiteral("pushdeer/cooldown"), 60).toInt());
    m_enabledCheck->setChecked(
        settings.value(QStringLiteral("general/auto_start"), false).toBool());

    onApplySettings();

    if (m_enabledCheck->isChecked()) {
        QTimer::singleShot(500, this, [this]() {
            if (!m_monitor->isRunning()) {
                onStartStop();
            }
        });
    }
}

void MainWindow::saveSettings()
{
    QSettings settings;
    settings.setValue(QStringLiteral("window/match"), m_windowMatchEdit->text());
    settings.setValue(QStringLiteral("window/threshold"), m_thresholdSpin->value());
    settings.setValue(QStringLiteral("pushdeer/key"), m_pushKeyEdit->text());
    settings.setValue(QStringLiteral("pushdeer/url"), m_pushUrlEdit->text());
    settings.setValue(QStringLiteral("pushdeer/message_template"),
                      m_messageTemplateEdit->toPlainText());
    settings.setValue(QStringLiteral("pushdeer/cooldown"), m_cooldownSpin->value());
    settings.setValue(QStringLiteral("general/auto_start"), m_enabledCheck->isChecked());
}

void MainWindow::onApplySettings()
{
    m_monitor->setWindowMatchString(m_windowMatchEdit->text());
    m_pushClient->setPushKey(m_pushKeyEdit->text());
    m_pushClient->setPushUrl(m_pushUrlEdit->text());
    saveSettings();
    m_statusLabel->setText(QStringLiteral("设置已保存"));
}

void MainWindow::onStartStop()
{
    if (m_monitor->isRunning()) {
        m_monitor->stop();
        m_startBtn->setText(QStringLiteral("开始监控"));
        m_statusLabel->setText(QStringLiteral("状态: 已停止"));
        m_countLabel->setText(QStringLiteral("当前匹配窗口数: --"));
        m_lastCount = -1;
    } else {
        if (m_windowMatchEdit->text().trimmed().isEmpty()) {
            QMessageBox::warning(this,
                QStringLiteral("配置错误"),
                QStringLiteral("请先填写窗口标题关键词"));
            return;
        }
        if (m_pushKeyEdit->text().trimmed().isEmpty()) {
            QMessageBox::warning(this,
                QStringLiteral("配置错误"),
                QStringLiteral("请先填写 PushDeer Key"));
            return;
        }

        m_lastAlertTime = QDateTime();
        m_monitor->setWindowMatchString(m_windowMatchEdit->text());
        m_pushClient->setPushKey(m_pushKeyEdit->text());
        m_pushClient->setPushUrl(m_pushUrlEdit->text());
        saveSettings();

        m_monitor->start(3000);
        m_startBtn->setText(QStringLiteral("停止监控"));
        m_statusLabel->setText(QStringLiteral("状态: 运行中 (每3秒检测)"));
    }
}

void MainWindow::onWindowCountChanged(int count)
{
    m_lastCount = count;
    m_countLabel->setText(
        QStringLiteral("当前匹配窗口数: %1").arg(count));

    int threshold = m_thresholdSpin->value();
    if (count >= threshold) {
        m_lastAlertTime = QDateTime();
    }
}

void MainWindow::onUiTick()
{
    if (!m_monitor->isRunning()) return;

    int count = m_lastCount;
    int threshold = m_thresholdSpin->value();
    int cooldown = m_cooldownSpin->value();

    if (count < 0) return;

    if (count >= threshold) {
        m_statusLabel->setText(
            QStringLiteral("状态: 运行中 - 正常 (窗口数 %1 ≥ %2)")
                .arg(count).arg(threshold));
        return;
    }

    QDateTime now = QDateTime::currentDateTime();

    if (!m_lastAlertTime.isValid()) {
        m_lastAlertTime = now;
        sendAlert(count);
        m_statusLabel->setText(
            QStringLiteral("⚠️ 警报: 窗口数 %1 低于阈值 %2 (已推送)")
                .arg(count).arg(threshold));
        return;
    }

    qint64 elapsed = m_lastAlertTime.secsTo(now);

    if (elapsed >= cooldown) {
        m_lastAlertTime = now;
        sendAlert(count);
        m_statusLabel->setText(
            QStringLiteral("⚠️ 警报: 窗口数 %1 低于阈值 %2 (已推送)")
                .arg(count).arg(threshold));
    } else {
        m_statusLabel->setText(
            QStringLiteral("⚠️ 窗口数 %1 低于阈值 %2 (冷却中，剩余 %3 秒)")
                .arg(count).arg(threshold)
                .arg(cooldown - elapsed));
    }
}

void MainWindow::sendAlert(int currentCount)
{
    int threshold = m_thresholdSpin->value();
    QString templateText = m_messageTemplateEdit->toPlainText();
    if (templateText.trimmed().isEmpty()) {
        templateText = QStringLiteral("⚠️ 游戏窗口不足！当前仅 {count} 个，低于阈值 {threshold} 个。");
    }

    QString message = templateText;
    message.replace(QStringLiteral("{count}"), QString::number(currentCount));
    message.replace(QStringLiteral("{threshold}"), QString::number(threshold));
    message.replace(QStringLiteral("{match}"), m_windowMatchEdit->text());
    message.replace(QStringLiteral("{time}"),
                    QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss")));

    m_pushClient->sendMessage(message);
}

void MainWindow::onTestPush()
{
    if (m_pushKeyEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this,
            QStringLiteral("配置错误"),
            QStringLiteral("请先填写 PushDeer Key"));
        return;
    }

    m_pushClient->setPushKey(m_pushKeyEdit->text());
    m_pushClient->setPushUrl(m_pushUrlEdit->text());
    m_pushClient->sendMessage(QStringLiteral("GameWatch 测试推送 - 如果收到此消息说明推送配置正常 👍"));
    m_statusLabel->setText(QStringLiteral("正在发送测试推送..."));
}

void MainWindow::onPushResult(bool success, const QString &error)
{
    if (success) {
        if (!m_monitor->isRunning()) {
            m_statusLabel->setText(QStringLiteral("测试推送成功"));
        }
    } else {
        m_statusLabel->setText(
            QStringLiteral("推送失败: %1").arg(error));
        QMessageBox::warning(this,
            QStringLiteral("推送失败"),
            QStringLiteral("PushDeer 推送失败:\n%1").arg(error));
    }
}