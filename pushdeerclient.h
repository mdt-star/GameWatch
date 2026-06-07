#ifndef PUSHDEERCLIENT_H
#define PUSHDEERCLIENT_H

#include <QObject>
#include <QString>

class PushDeerClient : public QObject
{
    Q_OBJECT

public:
    explicit PushDeerClient(QObject *parent = nullptr);
    ~PushDeerClient() override;

    void setPushKey(const QString &key);
    QString pushKey() const;

    void setPushUrl(const QString &url);
    QString pushUrl() const;

    /// Send a push notification synchronously (no Qt Network dependency).
    /// Uses WinHTTP on Windows, libcurl on macOS (via system curl).
    void sendMessage(const QString &text);

signals:
    void messageSent(bool success, const QString &errorMsg);

private:
    QString m_pushKey;
    QString m_pushUrl;
};

#endif // PUSHDEERCLIENT_H