#include "dd373dialog.h"
#include "dd373scraper.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QFileDialog>
#include <QTextStream>
#include <QFile>
#include <QDateTime>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPixmap>
#include <QApplication>

DD373Dialog::DD373Dialog(QWidget *parent)
    : QWidget(parent)
    , m_scraper(new DD373Scraper(this))
    , m_contextMenu(new QMenu(this))
{
    setupUI();

    connect(m_scraper, &DD373Scraper::itemsParsed,
            this, &DD373Dialog::onItemsParsed);
    connect(m_scraper, &DD373Scraper::errorOccurred,
            this, &DD373Dialog::onError);

    // Context menu actions
    QAction *openAct = m_contextMenu->addAction(QStringLiteral("打开链接"));
    connect(openAct, &QAction::triggered, this, &DD373Dialog::onOpenUrl);

    m_contextMenu->addSeparator();

    QAction *exportSelAct = m_contextMenu->addAction(QStringLiteral("导出选中到 CSV"));
    connect(exportSelAct, &QAction::triggered, this, &DD373Dialog::onExportSelected);

    m_contextMenu->addSeparator();

    QAction *filter10Act = m_contextMenu->addAction(QStringLiteral("过滤月卡到期 < 10 天"));
    connect(filter10Act, &QAction::triggered, this, &DD373Dialog::onFilterLessThan10Days);

    QAction *filter5Act = m_contextMenu->addAction(QStringLiteral("过滤月卡到期 < 5 天"));
    connect(filter5Act, &QAction::triggered, this, &DD373Dialog::onFilterLessThan5Days);

    m_contextMenu->addSeparator();

    QAction *clearFilterAct = m_contextMenu->addAction(QStringLiteral("清除过滤"));
    connect(clearFilterAct, &QAction::triggered, this, &DD373Dialog::onClearFilter);
}

DD373Dialog::~DD373Dialog() = default;

void DD373Dialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(8, 8, 8, 8);

    // Top bar: URL input + price filter + buttons
    QHBoxLayout *topLayout = new QHBoxLayout();

    topLayout->addWidget(new QLabel(QStringLiteral("URL:"), this));
    m_urlEdit = new QLineEdit(this);
    m_urlEdit->setPlaceholderText(QStringLiteral("dd373 商品列表页 URL"));
    m_urlEdit->setText(QStringLiteral(
        "https://www.dd373.com/s-q0kacj-khkbh6-0-0-0-0-qdu40u-0-0-0-5o17u7"
        "$dpq29h_dqljca$vvb4h4_ydasek$btuj8s-0-1-20-0-0.html"));
    topLayout->addWidget(m_urlEdit, 1);

    topLayout->addWidget(new QLabel(QStringLiteral("最高价:"), this));
    m_maxPriceSpin = new QSpinBox(this);
    m_maxPriceSpin->setRange(0, 99999);
    m_maxPriceSpin->setValue(200);
    m_maxPriceSpin->setSuffix(QStringLiteral(" 元"));
    topLayout->addWidget(m_maxPriceSpin);

    m_fetchBtn = new QPushButton(QStringLiteral("抓取"), this);
    topLayout->addWidget(m_fetchBtn);

    m_exportAllBtn = new QPushButton(QStringLiteral("导出全部"), this);
    topLayout->addWidget(m_exportAllBtn);

    mainLayout->addLayout(topLayout);

    // Status bar
    m_statusLabel = new QLabel(QStringLiteral("就绪"), this);
    mainLayout->addWidget(m_statusLabel);

    // Table
    m_table = new QTableWidget(this);
    m_table->setColumnCount(10);
    m_table->setHorizontalHeaderLabels({
        QStringLiteral("图片"),
        QStringLiteral("标题"),
        QStringLiteral("价格"),
        QStringLiteral("区服"),
        QStringLiteral("月卡到期"),
        QStringLiteral("剩余天数"),
        QStringLiteral("等级"),
        QStringLiteral("职业"),
        QStringLiteral("发布时间"),
        QStringLiteral("存疑")
    });

    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setDefaultSectionSize(80);

    connect(m_table, &QTableWidget::customContextMenuRequested,
            this, &DD373Dialog::onContextMenu);

    mainLayout->addWidget(m_table, 1);

    // Connect buttons
    connect(m_fetchBtn, &QPushButton::clicked, this, &DD373Dialog::onFetch);
    connect(m_exportAllBtn, &QPushButton::clicked, this, &DD373Dialog::onExportAll);
}

void DD373Dialog::onFetch()
{
    QString url = m_urlEdit->text().trimmed();
    if (url.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("请输入 URL"));
        return;
    }

    m_fetchBtn->setEnabled(false);
    m_fetchBtn->setText(QStringLiteral("抓取中..."));
    m_statusLabel->setText(QStringLiteral("正在抓取..."));
    m_table->setRowCount(0);
    m_items.clear();
    m_filteredItems.clear();

    m_scraper->fetchPage(url);
}

void DD373Dialog::onItemsParsed(const QList<DD373Item> &items)
{
    m_fetchBtn->setEnabled(true);
    m_fetchBtn->setText(QStringLiteral("抓取"));
    m_items = items;
    m_filteredItems = items;

    // Apply max price filter
    int maxPrice = m_maxPriceSpin->value();
    if (maxPrice > 0) {
        QList<DD373Item> filtered;
        for (const auto &item : items) {
            if (item.price <= maxPrice) {
                filtered.append(item);
            }
        }
        m_filteredItems = filtered;
    }

    m_statusLabel->setText(
        QStringLiteral("共 %1 个商品，低于 %2 元: %3 个")
            .arg(items.size())
            .arg(m_maxPriceSpin->value())
            .arg(m_filteredItems.size()));

    populateTable(m_filteredItems);
}

void DD373Dialog::onError(const QString &error)
{
    m_fetchBtn->setEnabled(true);
    m_fetchBtn->setText(QStringLiteral("抓取"));
    m_statusLabel->setText(QStringLiteral("错误: ") + error);
    QMessageBox::warning(this, QStringLiteral("抓取失败"), error);
}

void DD373Dialog::populateTable(const QList<DD373Item> &items)
{
    m_table->setRowCount(items.size());

    for (int i = 0; i < items.size(); ++i) {
        const DD373Item &item = items[i];

        // Image placeholder
        QLabel *imgLabel = new QLabel(this);
        imgLabel->setFixedSize(70, 70);
        imgLabel->setAlignment(Qt::AlignCenter);
        imgLabel->setText(QStringLiteral("加载中..."));
        imgLabel->setStyleSheet(QStringLiteral("background-color: #f0f0f0; border: 1px solid #ccc;"));
        m_table->setCellWidget(i, 0, imgLabel);

        if (!item.imageUrl.isEmpty()) {
            updateImage(i, item.imageUrl);
        }

        // Title
        QTableWidgetItem *titleItem = new QTableWidgetItem(item.title);
        titleItem->setToolTip(item.fullUrl);
        m_table->setItem(i, 1, titleItem);

        // Price
        QTableWidgetItem *priceItem = new QTableWidgetItem(
            QStringLiteral("￥%1").arg(item.price, 0, 'f', 2));
        priceItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_table->setItem(i, 2, priceItem);

        // Region / Server
        QString serverStr = item.region;
        if (!item.server.isEmpty()) {
            serverStr += " / " + item.server;
        }
        m_table->setItem(i, 3, new QTableWidgetItem(serverStr));

        // Monthly card expiry
        QString cardStr;
        if (!item.monthlyCardRaw.isEmpty()) {
            cardStr = item.monthlyCardRaw;
        } else if (item.monthlyCardDate.isValid()) {
            cardStr = item.monthlyCardDate.toString(QStringLiteral("yyyy/M/d"));
        } else {
            cardStr = QStringLiteral("无月卡");
        }
        QTableWidgetItem *cardItem = new QTableWidgetItem(cardStr);
        m_table->setItem(i, 4, cardItem);

        // Days left
        QString daysStr;
        if (item.monthlyCardDaysLeft >= 0) {
            daysStr = QString::number(item.monthlyCardDaysLeft) + QStringLiteral(" 天");
        } else if (item.monthlyCardDate.isValid()) {
            int days = daysUntilDate(item.monthlyCardDate);
            if (days >= 0) {
                daysStr = QString::number(days) + QStringLiteral(" 天");
            } else if (days < 0 && days != -1) {
                daysStr = QStringLiteral("已过期");
            } else {
                daysStr = QStringLiteral("未知");
            }
        } else {
            daysStr = QStringLiteral("未知");
        }
        QTableWidgetItem *daysItem = new QTableWidgetItem(daysStr);
        daysItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_table->setItem(i, 5, daysItem);

        // Level
        QTableWidgetItem *levelItem = new QTableWidgetItem(
            QString::number(item.level) + QStringLiteral(" 级"));
        levelItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_table->setItem(i, 6, levelItem);

        // Class
        m_table->setItem(i, 7, new QTableWidgetItem(item.characterClass));

        // Publish time
        QString pubStr;
        if (item.publishDt.isValid()) {
            pubStr = item.publishDt.toString(QStringLiteral("MM-dd hh:mm"));
        } else {
            pubStr = item.publishTime;
        }
        m_table->setItem(i, 8, new QTableWidgetItem(pubStr));

        // Doubtful flag
        QString doubtfulStr;
        if (item.monthlyCardDoubtful) {
            doubtfulStr = QStringLiteral("⚠️ 存疑");
        }
        QTableWidgetItem *doubtItem = new QTableWidgetItem(doubtfulStr);
        if (item.monthlyCardDoubtful) {
            doubtItem->setForeground(QColor(255, 100, 0));
        }
        m_table->setItem(i, 9, doubtItem);

        // Store the full URL as data in the first column
        m_table->item(i, 1)->setData(Qt::UserRole, item.fullUrl);
    }
}

int DD373Dialog::daysUntilDate(const QDate &date) const
{
    if (!date.isValid()) return -1;
    return QDate::currentDate().daysTo(date);
}

void DD373Dialog::updateImage(int row, const QString &imageUrl)
{
    // Simple synchronous image download using QNetworkAccessManager
    QNetworkAccessManager *manager = new QNetworkAccessManager(this);
    QNetworkRequest request{QUrl(imageUrl)};
    QNetworkReply *reply = manager->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply, row, manager]() {
        reply->deleteLater();
        manager->deleteLater();

        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QPixmap pixmap;
            if (pixmap.loadFromData(data)) {
                QLabel *label = qobject_cast<QLabel *>(m_table->cellWidget(row, 0));
                if (label) {
                    label->setPixmap(pixmap.scaled(70, 70, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                    label->setText(QString());
                }
            } else {
                QLabel *label = qobject_cast<QLabel *>(m_table->cellWidget(row, 0));
                if (label) label->setText(QStringLiteral("图"));
            }
        } else {
            QLabel *label = qobject_cast<QLabel *>(m_table->cellWidget(row, 0));
            if (label) label->setText(QStringLiteral("×"));
        }
    });
}

void DD373Dialog::onContextMenu(const QPoint &pos)
{
    QTableWidgetItem *item = m_table->itemAt(pos);
    if (!item) return;
    m_contextMenu->exec(m_table->viewport()->mapToGlobal(pos));
}

void DD373Dialog::onOpenUrl()
{
    QList<QTableWidgetItem *> selected = m_table->selectedItems();
    if (selected.isEmpty()) return;

    int row = selected.first()->row();
    QTableWidgetItem *titleItem = m_table->item(row, 1);
    if (!titleItem) return;

    QString url = titleItem->data(Qt::UserRole).toString();
    if (!url.isEmpty()) {
        QDesktopServices::openUrl(QUrl(url));
    }
}

void DD373Dialog::onExportSelected()
{
    QList<QTableWidgetItem *> selected = m_table->selectedItems();
    if (selected.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("导出"),
                                 QStringLiteral("请先选择要导出的行"));
        return;
    }

    // Get unique rows
    QSet<int> rows;
    for (auto *item : selected) {
        rows.insert(item->row());
    }

    QString fileName = QFileDialog::getSaveFileName(this,
        QStringLiteral("导出选中数据"),
        QStringLiteral("dd373_selected.csv"),
        QStringLiteral("CSV 文件 (*.csv)"));
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("错误"),
                             QStringLiteral("无法写入文件"));
        return;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    // BOM for Excel compatibility
    out << QChar(0xFEFF);

    // Header
    out << QStringLiteral("标题,价格,区服,月卡到期,等级,职业,发布时间,URL\n");

    for (int row : rows) {
        const DD373Item *item = nullptr;
        int idx = -1;
        // Find in filtered items by matching title
        QTableWidgetItem *titleItem = m_table->item(row, 1);
        if (!titleItem) continue;

        for (int i = 0; i < m_filteredItems.size(); ++i) {
            if (m_filteredItems[i].title == titleItem->text()) {
                item = &m_filteredItems[i];
                idx = i;
                break;
            }
        }
        if (!item) continue;

        QStringList fields;
        auto escape = [](const QString &s) -> QString {
            QString r = s;
            r.replace("\"", "\"\"");
            return "\"" + r + "\"";
        };
        fields << escape(item->title);
        fields << QString::number(item->price, 'f', 2);
        fields << escape(item->region + "/" + item->server);
        fields << escape(item->monthlyCardRaw);
        fields << QString::number(item->level);
        fields << escape(item->characterClass);
        fields << escape(item->publishTime);
        fields << escape(item->fullUrl);

        out << fields.join(",") << "\n";
    }

    file.close();
    m_statusLabel->setText(QStringLiteral("已导出 %1 行").arg(rows.size()));
}

void DD373Dialog::onExportAll()
{
    if (m_filteredItems.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("导出"),
                                 QStringLiteral("没有数据可导出"));
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(this,
        QStringLiteral("导出全部数据"),
        QStringLiteral("dd373_all.csv"),
        QStringLiteral("CSV 文件 (*.csv)"));
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("错误"),
                             QStringLiteral("无法写入文件"));
        return;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << QChar(0xFEFF);

    out << QStringLiteral("标题,价格,区服,月卡到期,剩余天数,等级,职业,发布时间,URL,存疑\n");

    for (const auto &item : m_filteredItems) {
        auto escape = [](const QString &s) -> QString {
            QString r = s;
            r.replace("\"", "\"\"");
            return "\"" + r + "\"";
        };

        QStringList fields;
        fields << escape(item.title);
        fields << QString::number(item.price, 'f', 2);
        fields << escape(item.region + "/" + item.server);
        fields << escape(item.monthlyCardRaw);

        QString daysStr;
        if (item.monthlyCardDaysLeft >= 0) {
            daysStr = QString::number(item.monthlyCardDaysLeft);
        } else if (item.monthlyCardDate.isValid()) {
            int days = daysUntilDate(item.monthlyCardDate);
            daysStr = (days >= 0) ? QString::number(days) : QStringLiteral("已过期");
        }
        fields << daysStr;

        fields << QString::number(item.level);
        fields << escape(item.characterClass);
        fields << escape(item.publishTime);
        fields << escape(item.fullUrl);
        fields << (item.monthlyCardDoubtful ? QStringLiteral("是") : QString());

        out << fields.join(",") << "\n";
    }

    file.close();
    m_statusLabel->setText(QStringLiteral("已导出 %1 行").arg(m_filteredItems.size()));
}

void DD373Dialog::onFilterLessThan10Days()
{
    if (m_items.isEmpty()) return;

    QList<DD373Item> filtered;
    for (const auto &item : m_items) {
        int days = -1;
        if (item.monthlyCardDaysLeft >= 0) {
            days = item.monthlyCardDaysLeft;
        } else if (item.monthlyCardDate.isValid()) {
            days = daysUntilDate(item.monthlyCardDate);
        }
        if (days >= 0 && days < 10) {
            filtered.append(item);
        }
    }

    m_filteredItems = filtered;
    m_statusLabel->setText(
        QStringLiteral("月卡到期 < 10 天: %1 个").arg(filtered.size()));
    populateTable(filtered);
}

void DD373Dialog::onFilterLessThan5Days()
{
    if (m_items.isEmpty()) return;

    QList<DD373Item> filtered;
    for (const auto &item : m_items) {
        int days = -1;
        if (item.monthlyCardDaysLeft >= 0) {
            days = item.monthlyCardDaysLeft;
        } else if (item.monthlyCardDate.isValid()) {
            days = daysUntilDate(item.monthlyCardDate);
        }
        if (days >= 0 && days < 5) {
            filtered.append(item);
        }
    }

    m_filteredItems = filtered;
    m_statusLabel->setText(
        QStringLiteral("月卡到期 < 5 天: %1 个").arg(filtered.size()));
    populateTable(filtered);
}

void DD373Dialog::onClearFilter()
{
    m_filteredItems = m_items;
    int maxPrice = m_maxPriceSpin->value();
    if (maxPrice > 0) {
        QList<DD373Item> filtered;
        for (const auto &item : m_items) {
            if (item.price <= maxPrice) filtered.append(item);
        }
        m_filteredItems = filtered;
    }
    m_statusLabel->setText(
        QStringLiteral("共 %1 个，低于 %2 元: %3 个")
            .arg(m_items.size()).arg(m_maxPriceSpin->value()).arg(m_filteredItems.size()));
    populateTable(m_filteredItems);
}