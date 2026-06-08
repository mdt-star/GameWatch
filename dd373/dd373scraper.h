#ifndef DD373SCRAPER_H
#define DD373SCRAPER_H

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QDate>
#include <QList>
#include "dd373item.h"

class DD373Scraper : public QObject
{
    Q_OBJECT

public:
    explicit DD373Scraper(QObject *parent = nullptr);
    ~DD373Scraper() override;

    void fetchPage(const QString &url);

    static QList<DD373Item> parseHtml(const QString &html, const QString &baseUrl,
                                       int &totalCount, int &totalPages);

    static QDate parseMonthlyCardDate(const QString &raw, bool &doubtful);
    static QDateTime parsePublishTime(const QString &raw);

signals:
    void itemsParsed(const QList<DD373Item> &items, int totalCount, int totalPages, int currentPage);
    void errorOccurred(const QString &error);

private:
    void doFetch(const QString &url);
};

#endif // DD373SCRAPER_H