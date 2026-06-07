#ifndef PUSHDEERCLIENT_H
#define PUSHDEERCLIENT_H

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>

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

    /// Send a push notification.
    /// The default PushDeer URL is https://api2.pushdeer.com/message/push
    void sendMessage(const QString &text);

signals:
    void messageSent(bool success, const QString &errorMsg);

private slots:
    void onReplyFinished(QNetworkReply *reply);

private:
    QNetworkAccessManager *m_nam;
    QString m_pushKey;
    QString m_pushUrl;
};

#endif // PUSHDEERCLIENT_H