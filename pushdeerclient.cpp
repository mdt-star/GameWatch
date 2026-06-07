#include "pushdeerclient.h"
#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>

// Platform-specific send implementation
// Returns true on success, sets responseBody
static bool platformSend(const std::string &url, std::string &responseBody, std::string &errorMsg);

PushDeerClient::PushDeerClient(QObject *parent)
    : QObject(parent)
    , m_pushUrl(QStringLiteral("https://api2.pushdeer.com/message/push"))
{
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

    // Build URL with query params
    QUrl url(m_pushUrl);
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("pushkey"), m_pushKey);
    query.addQueryItem(QStringLiteral("text"), text);
    url.setQuery(query);

    std::string responseBody;
    std::string errorMsg;
    QByteArray urlBytes = url.toString(QUrl::FullyEncoded).toUtf8();

    bool ok = platformSend(urlBytes.toStdString(), responseBody, errorMsg);

    if (!ok) {
        emit messageSent(false, QString::fromStdString(errorMsg));
        return;
    }

    // Parse JSON response
    QJsonDocument doc = QJsonDocument::fromJson(
        QByteArray(responseBody.data(), static_cast<int>(responseBody.size())));
    if (doc.isObject()) {
        QJsonObject obj = doc.object();
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

// ============================================================
// Platform-specific implementations
// ============================================================

#if defined(Q_OS_WIN32)
// Windows: use WinHTTP (Schannel TLS, no OpenSSL needed)
#include <windows.h>
#include <winhttp.h>

static std::wstring utf8ToWide(const std::string &utf8)
{
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(),
                                  static_cast<int>(utf8.size()), nullptr, 0);
    if (len <= 0) return {};
    std::wstring result(static_cast<size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(),
                        static_cast<int>(utf8.size()), &result[0], len);
    return result;
}

static bool platformSend(const std::string &url, std::string &responseBody, std::string &errorMsg)
{
    URL_COMPONENTSW urlComp = { sizeof(URL_COMPONENTSW) };
    urlComp.dwSchemeLength    = (DWORD)-1;
    urlComp.dwHostNameLength  = (DWORD)-1;
    urlComp.dwUrlPathLength   = (DWORD)-1;
    urlComp.dwExtraInfoLength = (DWORD)-1;

    std::wstring wurl = utf8ToWide(url);
    if (!WinHttpCrackUrl(wurl.c_str(), static_cast<DWORD>(wurl.size()), 0, &urlComp)) {
        errorMsg = "URL 解析失败";
        return false;
    }

    std::wstring hostName(urlComp.lpszHostName, urlComp.dwHostNameLength);
    std::wstring urlPath(urlComp.lpszUrlPath, urlComp.dwUrlPathLength);
    std::wstring extraInfo(urlComp.lpszExtraInfo, urlComp.dwExtraInfoLength);
    std::wstring fullPath = urlPath + extraInfo;
    bool useTls = (urlComp.nScheme == INTERNET_SCHEME_HTTPS);

    HINTERNET hSession = WinHttpOpen(L"GameWatch/1.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     nullptr, nullptr, 0);
    if (!hSession) {
        errorMsg = "WinHttpOpen 失败";
        return false;
    }

    HINTERNET hConnect = WinHttpConnect(hSession, hostName.c_str(), urlComp.nPort, 0);
    if (!hConnect) {
        errorMsg = "WinHttpConnect 失败";
        WinHttpCloseHandle(hSession);
        return false;
    }

    DWORD flags = useTls ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", fullPath.c_str(),
                                            nullptr, nullptr, nullptr, flags);
    if (!hRequest) {
        errorMsg = "WinHttpOpenRequest 失败";
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            nullptr, 0, 0, 0))
    {
        errorMsg = "WinHttpSendRequest 失败";
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        errorMsg = "WinHttpReceiveResponse 失败";
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Read response
    DWORD bytesAvailable = 0;
    responseBody.clear();
    while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
        std::vector<char> buffer(bytesAvailable);
        DWORD bytesRead = 0;
        if (WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead)) {
            responseBody.append(buffer.data(), bytesRead);
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return true;
}

#elif defined(Q_OS_MACOS)
// macOS: use system curl via popen
#include <cstdio>
#include <memory>

static bool platformSend(const std::string &url, std::string &responseBody, std::string &errorMsg)
{
    std::string cmd = "curl -s -G --data-urlencode \"pushkey=@-\" \"" + url + "\"";
    // Actually for PushDeer GET, the data is in the URL already, so just:
    cmd = "curl -s \"" + url + "\" 2>/dev/null";

    FILE *fp = popen(cmd.c_str(), "r");
    if (!fp) {
        errorMsg = "执行 curl 失败";
        return false;
    }

    char buf[4096];
    responseBody.clear();
    while (fgets(buf, sizeof(buf), fp)) {
        responseBody += buf;
    }
    int ret = pclose(fp);

    if (ret != 0) {
        errorMsg = "curl 返回错误码: " + std::to_string(ret);
        return false;
    }

    return true;
}

#else
// Fallback (Linux): same curl approach
#include <cstdio>

static bool platformSend(const std::string &url, std::string &responseBody, std::string &errorMsg)
{
    std::string cmd = "curl -s \"" + url + "\" 2>/dev/null";
    FILE *fp = popen(cmd.c_str(), "r");
    if (!fp) {
        errorMsg = "执行 curl 失败";
        return false;
    }

    char buf[4096];
    responseBody.clear();
    while (fgets(buf, sizeof(buf), fp)) {
        responseBody += buf;
    }
    int ret = pclose(fp);
    if (ret != 0) {
        errorMsg = "curl 返回错误码: " + std::to_string(ret);
        return false;
    }
    return true;
}
#endif