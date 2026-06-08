#ifndef DD373DIALOG_H
#define DD373DIALOG_H

#include <QWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QTimer>
#include <QMenu>
#include <QList>
#include <QMap>
#include <QPixmap>
#include "dd373item.h"

class DD373Scraper;
class PushDeerClient;

class DD373Dialog : public QWidget
{
    Q_OBJECT

public:
    explicit DD373Dialog(QWidget *parent = nullptr);
    ~DD373Dialog() override;

    void setPushDeerClient(PushDeerClient *client);
    bool isPushDeerConfigured() const;

signals:
    void statusMessage(const QString &msg);
    void notifyConfigRequired();

private slots:
    void onFetch();
    void onPrevPage();
    void onNextPage();
    void onItemsParsed(const QList<DD373Item> &items, int totalCount, int totalPages, int currentPage);
    void onError(const QString &error);
    void onContextMenu(const QPoint &pos);
    void onOpenUrl();
    void onExportSelected();
    void onExportAll();
    void onFilterLessThan10Days();
    void onFilterLessThan5Days();
    void onClearFilter();
    void onImageDoubleClicked(int row, int column);
    void onAutoRefreshTick();
    void onNotifyThresholdChanged(int value);
    void onQuickSetThreshold();

private:
    void setupUI();
    void populateTable(const QList<DD373Item> &items);
    void doFetchPage();
    void sendPushNotification(const QString &text);
    int daysUntilDate(const QDate &date) const;
    void updateImage(int row, const QString &imageUrl);

    DD373Scraper *m_scraper;
    PushDeerClient *m_pushClient = nullptr;
    QList<DD373Item> m_items;
    QList<DD373Item> m_filteredItems;

    int m_currentPage = 1;
    int m_totalPages = 1;
    int m_totalCount = 0;
    QString m_baseUrl;
    int m_notifyThreshold = 9999;

    QLineEdit     *m_urlEdit;
    QPushButton   *m_fetchBtn;
    QPushButton   *m_exportAllBtn;
    QTableWidget  *m_table;
    QMenu         *m_contextMenu;
    QMap<int, QPixmap> m_imageCache;
    QLabel        *m_pageLabel;
    QPushButton   *m_prevBtn;
    QPushButton   *m_nextBtn;
    QComboBox     *m_sortCombo;
    QSpinBox      *m_refreshSpin;
    QCheckBox     *m_autoRefreshCheck;
    QSpinBox      *m_notifySpin;
    QTimer        *m_autoRefreshTimer;
};

#endif