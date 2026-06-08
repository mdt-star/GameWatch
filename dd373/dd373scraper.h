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

    /// Fetch and parse the dd373 page. Results are emitted via itemsParsed.
    void fetchPage(const QString &url);

    /// Parse HTML string into items
    static QList<DD373Item> parseHtml(const QString &html, const QString &baseUrl);

    /// Parse monthly card date from various formats
    static QDate parseMonthlyCardDate(const QString &raw, bool &doubtful);

    /// Parse publish time string
    static QDateTime parsePublishTime(const QString &raw);

signals:
    void itemsParsed(const QList<DD373Item> &items);
    void errorOccurred(const QString &error);

private:
    void doFetch(const QString &url);
};

#endif // DD373SCRAPER_H