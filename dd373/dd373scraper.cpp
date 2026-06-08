#include "dd373scraper.h"

#include <QRegularExpression>
#include <QUrl>

// Platform-specific HTTP GET
static bool httpGet(const QString &url, QString &responseBody, QString &errorMsg);

DD373Scraper::DD373Scraper(QObject *parent)
    : QObject(parent)
{
}

DD373Scraper::~DD373Scraper() = default;

void DD373Scraper::fetchPage(const QString &url)
{
    doFetch(url);
}

void DD373Scraper::doFetch(const QString &url)
{
    QString html, error;
    if (!httpGet(url, html, error)) {
        emit errorOccurred(error);
        return;
    }

    // Extract base URL from the full URL for constructing absolute URLs
    QUrl qurl(url);
    QString baseUrl = qurl.scheme() + "://" + qurl.host();

    QList<DD373Item> items = parseHtml(html, baseUrl);
    emit itemsParsed(items);
}

QList<DD373Item> DD373Scraper::parseHtml(const QString &html, const QString &baseUrl)
{
    QList<DD373Item> items;

    // Match each goods-list-item block
    // We use a simpler approach: find each item block by matching the opening and closing tags
    QString remainder = html;
    QRegularExpression itemRe(
        QStringLiteral("<div\\s+class=\"goods-list-item\\s+zh-goods-item\">"),
        QRegularExpression::CaseInsensitiveOption);

    int pos = 0;
    QRegularExpressionMatch itemMatch;
    while ((itemMatch = itemRe.match(remainder, pos)).hasMatch()) {
        int start = itemMatch.capturedStart();
        // Find the matching closing div
        int depth = 1;
        int end = start + itemMatch.capturedLength();
        // We need to find the end of this item's div block
        // The item div closes with </div> after the zh-box area
        // Use a simple heuristic: find the pattern </div>\s*</div>\s*</div> after the item
        QRegularExpression endRe(QStringLiteral("</div>\\s*</div>\\s*</div>\\s*</div>\\s*</div>"));
        QRegularExpressionMatch endMatch = endRe.match(remainder, end);
        if (!endMatch.hasMatch()) {
            break;
        }
        end = endMatch.capturedEnd();

        QString block = remainder.mid(start, end - start);

        DD373Item item;

        // --- Title ---
        QRegularExpression titleRe(QStringLiteral(
            "class=\"game-account-flag\"[^>]*>\\s*<i[^>]*>\\s*</i>\\s*([^<]+)"));
        QRegularExpressionMatch m = titleRe.match(block);
        if (m.hasMatch()) {
            item.title = m.captured(1).trimmed();
        }

        // --- Detail URL ---
        QRegularExpression urlRe(QStringLiteral(
            "<a\\s+href=\"(/detail-[^\"]+)\""));
        m = urlRe.match(block);
        if (m.hasMatch()) {
            item.detailUrl = m.captured(1);
            item.fullUrl = baseUrl + item.detailUrl;
        }

        // --- Image URL ---
        QRegularExpression imgRe(QStringLiteral(
            "data-original=\"([^\"]+)\""));
        m = imgRe.match(block);
        if (m.hasMatch()) {
            QString imgSrc = m.captured(1);
            if (imgSrc.startsWith("//")) {
                imgSrc = "https:" + imgSrc;
            } else if (imgSrc.startsWith("/")) {
                imgSrc = baseUrl + imgSrc;
            }
            item.imageUrl = imgSrc;
        }

        // --- Region & Server ---
        QRegularExpression serverRe(QStringLiteral(
            "class=\"game-qufu-value\">([^<]+)"));
        m = serverRe.match(block);
        if (m.hasMatch()) {
            QString serverText = m.captured(1).trimmed();
            QStringList parts = serverText.split('/');
            if (parts.size() >= 1) item.region = parts[0].trimmed();
            if (parts.size() >= 2) item.server = parts[1].trimmed();
        }

        // --- Account Type ---
        QRegularExpression accTypeRe(QStringLiteral(
            "账号类型[^<]*<[^>]+>([^<]+)"));
        m = accTypeRe.match(block);
        if (m.hasMatch()) {
            item.accountType = m.captured(1).trimmed();
        }

        // --- Has Monthly Card ---
        QRegularExpression cardRe(QStringLiteral(
            "是否有月卡[^<]*<[^>]+>([^<]+)"));
        m = cardRe.match(block);
        if (m.hasMatch()) {
            QString val = m.captured(1).trimmed();
            item.hasMonthlyCard = (val == QStringLiteral("有月卡"));
        }

        // --- Monthly Card Expiry ---
        QRegularExpression cardExpRe(QStringLiteral(
            "月卡到期时间[^<]*<[^>]+>([^<]+)"));
        m = cardExpRe.match(block);
        if (m.hasMatch()) {
            item.monthlyCardRaw = m.captured(1).trimmed();
            bool doubtful = false;
            item.monthlyCardDate = parseMonthlyCardDate(item.monthlyCardRaw, doubtful);
            item.monthlyCardDoubtful = doubtful;
        }

        // --- Remaining days ---
        QRegularExpression daysRe(QStringLiteral(
            "剩余时间[^<]*<[^>]+>([^<]+)"));
        m = daysRe.match(block);
        if (m.hasMatch()) {
            QString daysText = m.captured(1).trimmed();
            // e.g. "30天"
            QRegularExpression daysNumRe(QStringLiteral("(\\d+)\\s*天"));
            QRegularExpressionMatch dm = daysNumRe.match(daysText);
            if (dm.hasMatch()) {
                item.monthlyCardDaysLeft = dm.captured(1).toInt();
            }
        }

        // --- Character Class ---
        QRegularExpression classRe(QStringLiteral(
            "职业[^<]*<[^>]+>([^<]+)"));
        m = classRe.match(block);
        if (m.hasMatch()) {
            item.characterClass = m.captured(1).trimmed();
        }

        // --- Level ---
        QRegularExpression levelRe(QStringLiteral(
            "等级[^<]*<[^>]+>([^<]+)"));
        m = levelRe.match(block);
        if (m.hasMatch()) {
            QString lvText = m.captured(1).trimmed();
            QRegularExpression lvNumRe(QStringLiteral("(\\d+)"));
            QRegularExpressionMatch lm = lvNumRe.match(lvText);
            if (lm.hasMatch()) {
                item.level = lm.captured(1).toInt();
            }
        }

        // --- Price ---
        QRegularExpression priceRe(QStringLiteral(
            "class=\"goods-price\">[^<]*<[^>]+>￥([0-9.]+)"));
        m = priceRe.match(block);
        if (m.hasMatch()) {
            item.price = m.captured(1).toDouble();
        }

        // --- Publish Time ---
        QRegularExpression timeRe(QStringLiteral(
            "发布时间[：:][^<]*<[^>]+>([^<]+)"));
        m = timeRe.match(block);
        if (m.hasMatch()) {
            item.publishTime = m.captured(1).trimmed();
            item.publishDt = parsePublishTime(item.publishTime);
        }

        // If we have monthlyCardRaw in "还剩X天" format but no explicit date,
        // calculate the expiry date from publish time
        if (!item.monthlyCardDate.isValid() && item.monthlyCardDaysLeft > 0 && item.publishDt.isValid()) {
            item.monthlyCardDate = item.publishDt.date().addDays(item.monthlyCardDaysLeft);
            item.monthlyCardDoubtful = true; // Mark as doubtful since it's estimated
        }

        item.valid = !item.title.isEmpty();
        if (item.valid) {
            items.append(item);
        }

        pos = end;
    }

    return items;
}

QDate DD373Scraper::parseMonthlyCardDate(const QString &raw, bool &doubtful)
{
    doubtful = false;
    QString s = raw.trimmed();
    if (s.isEmpty()) return {};

    int currentYear = QDate::currentDate().year();

    // Format: "2026/6/26"
    {
        QDate d = QDate::fromString(s, QStringLiteral("yyyy/M/d"));
        if (d.isValid()) return d;
    }
    // Format: "2026.7.4"
    {
        QDate d = QDate::fromString(s, QStringLiteral("yyyy.M.d"));
        if (d.isValid()) return d;
    }
    // Format: "2026年7月7日"
    {
        QRegularExpression re(QStringLiteral("(\\d{4})\\s*年\\s*(\\d{1,2})\\s*月\\s*(\\d{1,2})\\s*日?"));
        QRegularExpressionMatch m = re.match(s);
        if (m.hasMatch()) {
            QDate d(m.captured(1).toInt(), m.captured(2).toInt(), m.captured(3).toInt());
            if (d.isValid()) return d;
        }
    }
    // Format: "7月4号" or "7月4日" — no year prefix, assume current year
    {
        QRegularExpression re(QStringLiteral("(\\d{1,2})\\s*月\\s*(\\d{1,2})\\s*[号日]"));
        QRegularExpressionMatch m = re.match(s);
        if (m.hasMatch()) {
            QDate d(currentYear, m.captured(1).toInt(), m.captured(2).toInt());
            if (d.isValid()) return d;
        }
    }
    // Format: "7.4" — no year prefix
    {
        QRegularExpression re(QStringLiteral("^(\\d{1,2})\\.(\\d{1,2})$"));
        QRegularExpressionMatch m = re.match(s);
        if (m.hasMatch()) {
            QDate d(currentYear, m.captured(1).toInt(), m.captured(2).toInt());
            if (d.isValid()) return d;
        }
    }
    // Format: "7/4" — no year prefix
    {
        QRegularExpression re(QStringLiteral("^(\\d{1,2})/(\\d{1,2})$"));
        QRegularExpressionMatch m = re.match(s);
        if (m.hasMatch()) {
            QDate d(currentYear, m.captured(1).toInt(), m.captured(2).toInt());
            if (d.isValid()) return d;
        }
    }

    // For "还剩X天" or "X天" format, we can't directly parse a date.
    // This is handled in parseHtml by adding days to publish date.
    doubtful = true;
    return {};
}

QDateTime DD373Scraper::parsePublishTime(const QString &raw)
{
    QString s = raw.trimmed();
    if (s.isEmpty()) return {};

    // Format: "2026-06-08 08:24:51"
    QDateTime dt = QDateTime::fromString(s, QStringLiteral("yyyy-MM-dd hh:mm:ss"));
    if (dt.isValid()) return dt;

    // Format: "2026/06/08 08:24:51"
    dt = QDateTime::fromString(s, QStringLiteral("yyyy/MM/dd hh:mm:ss"));
    if (dt.isValid()) return dt;

    // Try other common formats
    dt = QDateTime::fromString(s, Qt::ISODate);
    return dt;
}

// ============================================================
// Platform-specific HTTP implementation
// ============================================================

#if defined(Q_OS_MACOS)
#include <cstdio>
#include <memory>

static bool httpGet(const QString &url, QString &responseBody, QString &errorMsg)
{
    // Use system curl with gzip support
    std::string cmd = "curl -s --compressed -L -H \"User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36\" \""
        + url.toStdString() + "\" 2>/dev/null";

    FILE *fp = popen(cmd.c_str(), "r");
    if (!fp) {
        errorMsg = QStringLiteral("执行 curl 失败");
        return false;
    }

    char buf[8192];
    QByteArray data;
    while (fgets(buf, sizeof(buf), fp)) {
        data.append(buf);
    }
    int ret = pclose(fp);

    if (ret != 0) {
        errorMsg = QStringLiteral("curl 返回错误码: %1").arg(ret);
        return false;
    }

    responseBody = QString::fromUtf8(data);
    return true;
}

#elif defined(Q_OS_WIN32)
#include <windows.h>
#include <winhttp.h>
#include <vector>

static std::wstring toWide(const std::string &utf8)
{
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(),
                                  static_cast<int>(utf8.size()), nullptr, 0);
    if (len <= 0) return {};
    std::wstring result(static_cast<size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(),
                        static_cast<int>(utf8.size()), &result[0], len);
    return result;
}

static bool httpGet(const QString &url, QString &responseBody, QString &errorMsg)
{
    std::string urlStr = url.toStdString();

    URL_COMPONENTSW urlComp = { sizeof(URL_COMPONENTSW) };
    urlComp.dwSchemeLength    = (DWORD)-1;
    urlComp.dwHostNameLength  = (DWORD)-1;
    urlComp.dwUrlPathLength   = (DWORD)-1;
    urlComp.dwExtraInfoLength = (DWORD)-1;

    std::wstring wurl = toWide(urlStr);
    if (!WinHttpCrackUrl(wurl.c_str(), static_cast<DWORD>(wurl.size()), 0, &urlComp)) {
        errorMsg = QStringLiteral("URL 解析失败");
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
        errorMsg = QStringLiteral("WinHttpOpen 失败");
        return false;
    }

    // Accept gzip encoding
    DWORD acceptTypes[] = { 0 };
    HINTERNET hConnect = WinHttpConnect(hSession, hostName.c_str(), urlComp.nPort, 0);
    if (!hConnect) {
        errorMsg = QStringLiteral("WinHttpConnect 失败");
        WinHttpCloseHandle(hSession);
        return false;
    }

    DWORD flags = useTls ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", fullPath.c_str(),
                                            nullptr, nullptr, nullptr, flags);
    if (!hRequest) {
        errorMsg = QStringLiteral("WinHttpOpenRequest 失败");
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Set accept headers
    LPCWSTR headers = L"Accept-Encoding: gzip, deflate\r\nUser-Agent: Mozilla/5.0\r\n";
    WinHttpAddRequestHeaders(hRequest, headers, (ULONG)-1L, WINHTTP_ADDREQ_FLAG_ADD);

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            nullptr, 0, 0, 0))
    {
        errorMsg = QStringLiteral("WinHttpSendRequest 失败");
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        errorMsg = QStringLiteral("WinHttpReceiveResponse 失败");
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Read response
    DWORD bytesAvailable = 0;
    QByteArray data;
    while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
        std::vector<char> buffer(bytesAvailable);
        DWORD bytesRead = 0;
        if (WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead)) {
            data.append(buffer.data(), static_cast<int>(bytesRead));
        }
    }

    responseBody = QString::fromUtf8(data);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return true;
}

#else
// Linux fallback
#include <cstdio>

static bool httpGet(const QString &url, QString &responseBody, QString &errorMsg)
{
    std::string cmd = "curl -s --compressed -L \"" + url.toStdString() + "\" 2>/dev/null";
    FILE *fp = popen(cmd.c_str(), "r");
    if (!fp) {
        errorMsg = QStringLiteral("执行 curl 失败");
        return false;
    }

    char buf[8192];
    QByteArray data;
    while (fgets(buf, sizeof(buf), fp)) {
        data.append(buf);
    }
    int ret = pclose(fp);
    if (ret != 0) {
        errorMsg = QStringLiteral("curl 返回错误码: %1").arg(ret);
        return false;
    }

    responseBody = QString::fromUtf8(data);
    return true;
}
#endif