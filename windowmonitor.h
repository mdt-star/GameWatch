#ifndef WINDOWMONITOR_H
#define WINDOWMONITOR_H

#include <QObject>
#include <QString>
#include <QTimer>
#include <QRegularExpression>

class WindowMonitor : public QObject
{
    Q_OBJECT

public:
    explicit WindowMonitor(QObject *parent = nullptr);
    ~WindowMonitor() override;

    void setWindowMatchString(const QString &str);
    QString windowMatchString() const;

    void start(int intervalMs = 3000);
    void stop();
    bool isRunning() const;

    // Returns the number of windows matching the current pattern
    int matchingWindowCount() const;

signals:
    void windowCountChanged(int count);
    void alertTriggered(int currentCount, int threshold);

private slots:
    void onTick();

private:
    // Platform-specific: enumerate all window titles
    static QStringList allWindowTitles();

    QTimer *m_timer;
    QString m_matchString;
    int m_lastCount;
};

#endif // WINDOWMONITOR_H