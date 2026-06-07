#include "pushdeerclient.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QUrl>

PushDeerClient::PushDeerClient(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_pushUrl(QStringLiteral("https://api2.pushdeer.com/message/push"))
{
    connect(m_nam, &QNetworkAccessManager::finished,
            this, &PushDeerClient::onReplyFinished);
}

PushDeerClient::~PushDeerClient() = default;

void PushDeerClient::setPushKey(const QString &key)
{
    m_pushKey = key;
}

QString PushDeerClient::pushKey() const
{
    return m_pushKey;
}

void PushDeerClient::setPushUrl(const QString &url)
{
    m_pushUrl = url;
}

QString PushDeerClient::pushUrl() const
{
    return m_pushUrl;
}

void PushDeerClient::sendMessage(const QString &text)
{
    if (m_pushKey.isEmpty()) {
        emit messageSent(false, QStringLiteral("PushDeer Key 未设置"));
        return;
    }

    QUrl url(m_pushUrl);
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("pushkey"), m_pushKey);
    query.addQueryItem(QStringLiteral("text"), text);
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));

    // PushDeer API uses GET with query params
    m_nam->get(request);
}

void PushDeerClient::onReplyFinished(QNetworkReply *reply)
{
    if (!reply) {
        emit messageSent(false, QStringLiteral("网络请求失败：无响应"));
        return;
    }

    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        emit messageSent(false, reply->errorString());
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isObject()) {
        QJsonObject obj = doc.object();
        // PushDeer returns {"code":0,"content":{"result":["..."]}} on success
        int code = obj.value(QStringLiteral("code")).toInt(-1);
        if (code == 0) {
            emit messageSent(true, QString());
        } else {
            QString error = obj.value(QStringLiteral("error")).toString(
                QStringLiteral("推送失败 (code: %1)").arg(code));
            emit messageSent(false, error);
        }
    } else {
        emit messageSent(false, QStringLiteral("推送返回数据格式异常"));
    }
}