#include "windowmonitor.h"

// macOS: implementation in windowmonitor_mac.mm
// Windows: implementation in windowmonitor_win.cpp
// Other platforms: provide a stub fallback
#if !defined(Q_OS_MACOS) && !defined(Q_OS_WIN32)

QStringList WindowMonitor::allWindowTitles()
{
    // Fallback for non-macOS/non-Windows platforms (Linux)
    return {};
}

#endif

WindowMonitor::WindowMonitor(QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
    , m_lastCount(-1)
{
    connect(m_timer, &QTimer::timeout, this, &WindowMonitor::onTick);
}

WindowMonitor::~WindowMonitor() = default;

void WindowMonitor::setWindowMatchString(const QString &str)
{
    m_matchString = str;
}

QString WindowMonitor::windowMatchString() const
{
    return m_matchString;
}

void WindowMonitor::start(int intervalMs)
{
    if (m_matchString.isEmpty()) {
        return;
    }
    // Do an immediate check
    onTick();
    m_timer->start(intervalMs);
}

void WindowMonitor::stop()
{
    m_timer->stop();
}

bool WindowMonitor::isRunning() const
{
    return m_timer->isActive();
}

int WindowMonitor::matchingWindowCount() const
{
    if (m_matchString.isEmpty()) {
        return 0;
    }

    QStringList titles = allWindowTitles();
    QRegularExpression re(m_matchString, QRegularExpression::CaseInsensitiveOption);

    int count = 0;
    for (const QString &title : titles) {
        if (title.contains(re)) {
            count++;
        }
    }
    return count;
}

void WindowMonitor::onTick()
{
    int count = matchingWindowCount();
    m_lastCount = count;
    emit windowCountChanged(count);
}