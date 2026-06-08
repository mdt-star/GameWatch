#ifndef DD373DIALOG_H
#define DD373DIALOG_H

#include <QWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>
#include <QMenu>
#include <QList>
#include "dd373item.h"

class DD373Scraper;

class DD373Dialog : public QWidget
{
    Q_OBJECT

public:
    explicit DD373Dialog(QWidget *parent = nullptr);
    ~DD373Dialog() override;

signals:
    void statusMessage(const QString &msg);

private slots:
    void onFetch();
    void onItemsParsed(const QList<DD373Item> &items);
    void onError(const QString &error);
    void onContextMenu(const QPoint &pos);
    void onOpenUrl();
    void onExportSelected();
    void onExportAll();
    void onFilterLessThan10Days();
    void onFilterLessThan5Days();
    void onClearFilter();

private:
    void setupUI();
    void populateTable(const QList<DD373Item> &items);
    int daysUntilDate(const QDate &date) const;
    void updateImage(int row, const QString &imageUrl);

    DD373Scraper *m_scraper;
    QList<DD373Item> m_items;
    QList<DD373Item> m_filteredItems;

    // UI
    QLineEdit     *m_urlEdit;
    QSpinBox      *m_maxPriceSpin;
    QPushButton   *m_fetchBtn;
    QPushButton   *m_exportAllBtn;
    QLabel        *m_statusLabel;
    QTableWidget  *m_table;
    QMenu         *m_contextMenu;
};

#endif // DD373DIALOG_H