#include "dd373scraper.h"

#include <QRegularExpression>
#include <QUrl>

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

    QUrl qurl(url);
    QString baseUrl = qurl.scheme() + "://" + qurl.host();

    int totalCount = 0, totalPages = 0;
    QList<DD373Item> items = parseHtml(html, baseUrl, totalCount, totalPages);

    // Parse current page from HTML
    int currentPage = 1;
    QRegularExpression pageRe(QStringLiteral(
        "class=\"current-index page-index\"[^>]*value=\"(\\d+)\""));
    QRegularExpressionMatch pm = pageRe.match(html);
    if (pm.hasMatch()) {
        currentPage = pm.captured(1).toInt();
    }

    // Parse total pages from HTML: <span class="max-number">12</span>
    QRegularExpression totalRe(QStringLiteral(
        "class=\"[^\"]*max-number[^\"]*\">(\\d+)<"));
    QRegularExpressionMatch tm = totalRe.match(html);
    if (tm.hasMatch()) {
        totalPages = tm.captured(1).toInt();
    }

    // Parse total count: >224</span>条 records
    QRegularExpression countRe(QStringLiteral(">(\\d+)</span>[^<]*条"));
    QRegularExpressionMatch cm = countRe.match(html);
    if (cm.hasMatch()) {
        totalCount = cm.captured(1).toInt();
    }
    emit itemsParsed(items, totalCount, totalPages, currentPage);
}

QList<DD373Item> DD373Scraper::parseHtml(const QString &html, const QString &baseUrl,
                                           int &totalCount, int &totalPages)
{
    QList<DD373Item> items;
    (void)totalCount; // computed elsewhere
    (void)totalPages;

    QString remainder = html;
    QRegularExpression itemRe(
        QStringLiteral("<div\\s+class=\"goods-list-item\\s+zh-goods-item\">"),
        QRegularExpression::CaseInsensitiveOption);

    int pos = 0;
    QRegularExpressionMatch itemMatch;
    while ((itemMatch = itemRe.match(remainder, pos)).hasMatch()) {
        int start = itemMatch.capturedStart();
        int searchPos = start + itemMatch.capturedLength();

        int end = -1;
        QRegularExpression tagRe(QStringLiteral("<(/?)div[^>]*>"),
                                 QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatchIterator it = tagRe.globalMatch(remainder, searchPos);
        int depth = 1;

        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            if (m.captured(1).isEmpty()) depth++;
            else { depth--; if (depth == 0) { end = m.capturedEnd(); break; } }
        }
        if (end < 0) break;

        QString block = remainder.mid(start, end - start);
        DD373Item item;
        QRegularExpressionMatch m;

        // Title
        QRegularExpression titleRe(QStringLiteral(
            "game-account-flag[^>]*>\\s*<i[^>]*>\\s*</i>\\s*([^<]+)"));
        m = titleRe.match(block);
        if (m.hasMatch()) item.title = m.captured(1).trimmed();

        // Detail URL
        QRegularExpression urlRe(QStringLiteral("href=\"(/detail-[^\"]+)\""));
        m = urlRe.match(block);
        if (m.hasMatch()) { item.detailUrl = m.captured(1); item.fullUrl = baseUrl + item.detailUrl; }

        // Image URL
        QRegularExpression imgRe(QStringLiteral("data-original=\"([^\"]+)\""));
        m = imgRe.match(block);
        if (m.hasMatch()) {
            QString imgSrc = m.captured(1);
            if (imgSrc.startsWith("//")) imgSrc = "https:" + imgSrc;
            else if (imgSrc.startsWith("/")) imgSrc = baseUrl + imgSrc;
            item.imageUrl = imgSrc;
        }

        // Region & Server
        QRegularExpression serverRe(QStringLiteral("game-qufu-value\">([\\s\\S]*?)</span>"));
        m = serverRe.match(block);
        if (m.hasMatch()) {
            QString serverHtml = m.captured(1);
            QRegularExpression aRe(QStringLiteral(">([^<]+)</a>"));
            QStringList parts;
            QRegularExpressionMatchIterator ait = aRe.globalMatch(serverHtml);
            while (ait.hasNext()) parts << ait.next().captured(1).trimmed();
            if (parts.size() >= 1) item.region = parts[0];
            if (parts.size() >= 2) item.server = parts[1];
        }

        // label-value extractor
        auto extractLabel = [&](const QString &label) -> QString {
            QRegularExpression re(label + QLatin1String("[\\s\\S]*?<[^>]*>[\\s\\S]*?<[^>]*>([^<]+)"));
            QRegularExpressionMatch rm = re.match(block);
            if (rm.hasMatch()) return rm.captured(1).trimmed();
            return QString();
        };

        item.accountType = extractLabel(QStringLiteral("账号类型"));
        QString cardVal = extractLabel(QStringLiteral("是否有月卡"));
        item.hasMonthlyCard = (cardVal == QStringLiteral("有月卡"));

        QString cardExpStr = extractLabel(QStringLiteral("月卡到期时间"));
        if (!cardExpStr.isEmpty()) {
            item.monthlyCardRaw = cardExpStr;
            bool doubtful = true;
            item.monthlyCardDate = parseMonthlyCardDate(cardExpStr, doubtful);
            item.monthlyCardDoubtful = doubtful;
        }

        QString daysText = extractLabel(QStringLiteral("剩余时间"));
        if (!daysText.isEmpty()) {
            QRegularExpression daysNumRe(QStringLiteral("(\\d+)\\s*天"));
            QRegularExpressionMatch dm = daysNumRe.match(daysText);
            if (dm.hasMatch()) item.monthlyCardDaysLeft = dm.captured(1).toInt();
        }

        item.characterClass = extractLabel(QStringLiteral("职业"));

        QString lvText = extractLabel(QStringLiteral("等级"));
        if (!lvText.isEmpty()) {
            QRegularExpression lvNumRe(QStringLiteral("(\\d+)"));
            QRegularExpressionMatch lm = lvNumRe.match(lvText);
            if (lm.hasMatch()) item.level = lm.captured(1).toInt();
        }

        // Price
        QRegularExpression priceRe(QStringLiteral("goods-price\">\\s*<[^>]*>￥([0-9.]+)"));
        m = priceRe.match(block);
        if (m.hasMatch()) item.price = m.captured(1).toDouble();

        // Publish Time
        QRegularExpression timeRe(QStringLiteral("发布时间[：:][\\s\\S]*?<[^>]+>[\\s\\S]*?<[^>]+>([^<]+)"));
        m = timeRe.match(block);
        if (m.hasMatch()) { item.publishTime = m.captured(1).trimmed(); item.publishDt = parsePublishTime(item.publishTime); }

        // Calculate expiry from publish time if only "还剩X天"
        if (!item.monthlyCardDate.isValid() && item.monthlyCardDaysLeft > 0 && item.publishDt.isValid()) {
            item.monthlyCardDate = item.publishDt.date().addDays(item.monthlyCardDaysLeft);
            item.monthlyCardDoubtful = true;
        } else if (!item.monthlyCardDate.isValid() && item.monthlyCardDaysLeft > 0) {
            item.monthlyCardDoubtful = true;
        }

        item.valid = !item.title.isEmpty();
        if (item.valid) items.append(item);
        pos = end;
    }

    return items;
}

QDate DD373Scraper::parseMonthlyCardDate(const QString &raw, bool &doubtful)
{
    doubtful = true;
    QString s = raw.trimmed();
    if (s.isEmpty()) return {};
    int currentYear = QDate::currentDate().year();

    { QDate d = QDate::fromString(s, QStringLiteral("yyyy/M/d")); if (d.isValid()) { doubtful = false; return d; } }
    { QDate d = QDate::fromString(s, QStringLiteral("yyyy.M.d")); if (d.isValid()) { doubtful = false; return d; } }
    { QRegularExpression re(QStringLiteral("^(\\d{4})(\\d{2})(\\d{2})$"));
      QRegularExpressionMatch m = re.match(s);
      if (m.hasMatch()) { QDate d(m.captured(1).toInt(), m.captured(2).toInt(), m.captured(3).toInt()); if (d.isValid()) { doubtful = false; return d; } } }
    { QRegularExpression re(QStringLiteral("(\\d{4})\\s*年\\s*(\\d{1,2})\\s*月\\s*(\\d{1,2})\\s*日?"));
      QRegularExpressionMatch m = re.match(s);
      if (m.hasMatch()) { QDate d(m.captured(1).toInt(), m.captured(2).toInt(), m.captured(3).toInt()); if (d.isValid()) { doubtful = false; return d; } } }
    // no year
    { QRegularExpression re(QStringLiteral("(\\d{1,2})\\s*月\\s*(\\d{1,2})\\s*[号日]"));
      QRegularExpressionMatch m = re.match(s);
      if (m.hasMatch()) { QDate d(currentYear, m.captured(1).toInt(), m.captured(2).toInt()); if (d.isValid()) return d; } }
    { QRegularExpression re(QStringLiteral("^(\\d{1,2})\\.(\\d{1,2})日?$"));
      QRegularExpressionMatch m = re.match(s);
      if (m.hasMatch()) { QDate d(currentYear, m.captured(1).toInt(), m.captured(2).toInt()); if (d.isValid()) return d; } }
    { QRegularExpression re(QStringLiteral("^(\\d{1,2})/(\\d{1,2})$"));
      QRegularExpressionMatch m = re.match(s);
      if (m.hasMatch()) { QDate d(currentYear, m.captured(1).toInt(), m.captured(2).toInt()); if (d.isValid()) return d; } }

    return {};
}

QDateTime DD373Scraper::parsePublishTime(const QString &raw)
{
    QString s = raw.trimmed();
    if (s.isEmpty()) return {};
    QDateTime dt = QDateTime::fromString(s, QStringLiteral("yyyy-MM-dd hh:mm:ss"));
    if (dt.isValid()) return dt;
    dt = QDateTime::fromString(s, QStringLiteral("yyyy/MM/dd hh:mm:ss"));
    if (dt.isValid()) return dt;
    return QDateTime::fromString(s, Qt::ISODate);
}

// ============================================================
// Platform HTTP
// ============================================================
#if defined(Q_OS_MACOS)
#include <cstdio>
static bool httpGet(const QString &url, QString &responseBody, QString &errorMsg) {
    std::string cmd = "curl -s --compressed -L -H 'User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36' --url '" + url.toStdString() + "' 2>/dev/null";
    FILE *fp = popen(cmd.c_str(), "r");
    if (!fp) { errorMsg = QStringLiteral("curl失败"); return false; }
    char buf[8192]; QByteArray data;
    while (fgets(buf,sizeof(buf),fp)) data.append(buf);
    int ret = pclose(fp);
    if (ret != 0) { errorMsg = QStringLiteral("curl返回%1").arg(ret); return false; }
    responseBody = QString::fromUtf8(data); return true;
}
#elif defined(Q_OS_WIN32)
#include <windows.h> #include <winhttp.h> #include <vector>
static std::wstring toWide(const std::string &u) {
    int l = MultiByteToWideChar(CP_UTF8,0,u.c_str(),(int)u.size(),0,0);
    if(l<=0) return {}; std::wstring r((size_t)l,0); MultiByteToWideChar(CP_UTF8,0,u.c_str(),(int)u.size(),&r[0],l); return r;
}
static bool httpGet(const QString &url, QString &responseBody, QString &errorMsg) {
    std::string urlStr = url.toStdString();
    URL_COMPONENTSW uc={sizeof(URL_COMPONENTSW)}; uc.dwSchemeLength=uc.dwHostNameLength=uc.dwUrlPathLength=uc.dwExtraInfoLength=(DWORD)-1;
    std::wstring wurl=toWide(urlStr);
    if(!WinHttpCrackUrl(wurl.c_str(),(DWORD)wurl.size(),0,&uc)){errorMsg=QStringLiteral("URL解析失败");return false;}
    std::wstring host(uc.lpszHostName,uc.dwHostNameLength),path(uc.lpszUrlPath,uc.dwUrlPathLength),extra(uc.lpszExtraInfo,uc.dwExtraInfoLength),full=path+extra;
    bool tls=(uc.nScheme==INTERNET_SCHEME_HTTPS);
    HINTERNET hS=WinHttpOpen(L"GameWatch/1.0",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,0,0,0);
    if(!hS){errorMsg=QStringLiteral("WinHttpOpen失败");return false;}
    HINTERNET hC=WinHttpConnect(hS,host.c_str(),uc.nPort,0);
    if(!hC){errorMsg=QStringLiteral("WinHttpConnect失败");WinHttpCloseHandle(hS);return false;}
    HINTERNET hR=WinHttpOpenRequest(hC,L"GET",full.c_str(),0,0,0,tls?WINHTTP_FLAG_SECURE:0);
    if(!hR){errorMsg=QStringLiteral("WinHttpOpenRequest失败");WinHttpCloseHandle(hC);WinHttpCloseHandle(hS);return false;}
    LPCWSTR hdrs=L"Accept-Encoding: gzip, deflate\r\nUser-Agent: Mozilla/5.0\r\n";
    WinHttpAddRequestHeaders(hR,hdrs,(ULONG)-1L,WINHTTP_ADDREQ_FLAG_ADD);
    if(!WinHttpSendRequest(hR,WINHTTP_NO_ADDITIONAL_HEADERS,0,0,0,0,0)){errorMsg=QStringLiteral("发送失败");WinHttpCloseHandle(hR);WinHttpCloseHandle(hC);WinHttpCloseHandle(hS);return false;}
    if(!WinHttpReceiveResponse(hR,0)){errorMsg=QStringLiteral("接收失败");WinHttpCloseHandle(hR);WinHttpCloseHandle(hC);WinHttpCloseHandle(hS);return false;}
    DWORD ba=0; QByteArray data;
    while(WinHttpQueryDataAvailable(hR,&ba)&&ba>0){std::vector<char>buf(ba);DWORD br=0;if(WinHttpReadData(hR,buf.data(),ba,&br))data.append(buf.data(),(int)br);}
    responseBody=QString::fromUtf8(data);
    WinHttpCloseHandle(hR);WinHttpCloseHandle(hC);WinHttpCloseHandle(hS);return true;
}
#else
#include <cstdio>
static bool httpGet(const QString &url, QString &responseBody, QString &errorMsg) {
    std::string cmd = "curl -s --compressed -L \"" + url.toStdString() + "\" 2>/dev/null";
    FILE *fp = popen(cmd.c_str(),"r"); if(!fp){errorMsg=QStringLiteral("curl失败");return false;}
    char buf[8192]; QByteArray data; while(fgets(buf,sizeof(buf),fp))data.append(buf);
    int ret=pclose(fp); if(ret!=0){errorMsg=QStringLiteral("curl返回%1").arg(ret);return false;}
    responseBody=QString::fromUtf8(data); return true;
}
#endif