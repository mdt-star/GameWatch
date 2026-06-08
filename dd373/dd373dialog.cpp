#include "dd373dialog.h"
#include "dd373scraper.h"
#include "pushdeerclient.h"

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
#include <QScreen>

DD373Dialog::DD373Dialog(QWidget *parent)
    : QWidget(parent)
    , m_scraper(new DD373Scraper(this))
    , m_contextMenu(new QMenu(this))
    , m_autoRefreshTimer(new QTimer(this))
{
    setupUI();

    connect(m_scraper, &DD373Scraper::itemsParsed, this, &DD373Dialog::onItemsParsed);
    connect(m_scraper, &DD373Scraper::errorOccurred, this, &DD373Dialog::onError);
    connect(m_autoRefreshTimer, &QTimer::timeout, this, &DD373Dialog::onAutoRefreshTick);
    connect(m_notifySpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &DD373Dialog::onNotifyThresholdChanged);

    // 自动刷新复选框：勾选禁止修改间隔、立即开始抓取并启动定时器
    connect(m_autoRefreshCheck, &QCheckBox::toggled, this, [this](bool checked) {
        m_refreshSpin->setDisabled(checked);
        if (checked) {
            if (!m_baseUrl.isEmpty() && !m_items.isEmpty()) {
                m_autoRefreshTimer->start(m_refreshSpin->value() * 1000);
                emit statusMessage(QStringLiteral("自动刷新已开启"));
            } else {
                // 没有数据时立即触发一次抓取，抓取成功后在 onItemsParsed 中启动定时器
                emit statusMessage(QStringLiteral("自动刷新已开启，正在抓取数据..."));
                onFetch();
            }
        } else {
            m_autoRefreshTimer->stop();
            emit statusMessage(QStringLiteral("自动刷新已关闭"));
        }
    });

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

void DD373Dialog::setPushDeerClient(PushDeerClient *client) { m_pushClient = client; }
bool DD373Dialog::isPushDeerConfigured() const { return m_pushClient && !m_pushClient->pushKey().isEmpty(); }
void DD373Dialog::onNotifyThresholdChanged(int value) { m_notifyThreshold = value; }

void DD373Dialog::onQuickSetThreshold()
{
    int val = 9999;
    if (m_totalCount > 0)
        val = m_totalCount;
    m_notifySpin->setValue(val);
    emit statusMessage(QStringLiteral("阈值已设为 %1").arg(val));
}

void DD373Dialog::setupUI()
{
    QVBoxLayout *ml = new QVBoxLayout(this);
    ml->setSpacing(6); ml->setContentsMargins(8, 8, 8, 8);

    // 第一行：URL + 抓取按钮
    QHBoxLayout *r1 = new QHBoxLayout();
    r1->addWidget(new QLabel(QStringLiteral("URL:"), this));
    m_urlEdit = new QLineEdit(this);
    m_urlEdit->setPlaceholderText(QStringLiteral("dd373 URL，用 {page}/{sort} 表示页码和排序"));
    m_urlEdit->setText(QStringLiteral("https://www.dd373.com/s-q0kacj-khkbh6-0-0-0-0-qdu40u-0-0-0-5o17u7$dpq29h_dqljca$vvb4h4_ydasek$btuj8s-0-{page}-20-{sort}-0.html"));
    r1->addWidget(m_urlEdit, 1);
    m_fetchBtn = new QPushButton(QStringLiteral("抓取"), this);
    r1->addWidget(m_fetchBtn);
    ml->addLayout(r1);

    // 第二行：自动刷新 + 阈值通知
    QHBoxLayout *r2 = new QHBoxLayout();
    r2->addWidget(new QLabel(QStringLiteral("自动刷新:"), this));
    m_autoRefreshCheck = new QCheckBox(this); r2->addWidget(m_autoRefreshCheck);
    m_refreshSpin = new QSpinBox(this);
    m_refreshSpin->setRange(1, 3600); m_refreshSpin->setValue(60);
    m_refreshSpin->setSuffix(QStringLiteral(" 秒")); m_refreshSpin->setFixedWidth(80);
    m_refreshSpin->setEnabled(true);
    r2->addWidget(m_refreshSpin);
    r2->addSpacing(20);
    r2->addWidget(new QLabel(QStringLiteral("大于"), this));
    m_notifySpin = new QSpinBox(this);
    m_notifySpin->setRange(1, 99999); m_notifySpin->setValue(9999);
    m_notifySpin->setSuffix(QStringLiteral(" 条通知我")); m_notifySpin->setFixedWidth(130);
    r2->addWidget(m_notifySpin);
    QPushButton *quickBtn = new QPushButton(QStringLiteral("设为当前商品量"), this);
    quickBtn->setToolTip(QStringLiteral("将当前数据条数填入阈值输入框"));
    connect(quickBtn, &QPushButton::clicked, this, &DD373Dialog::onQuickSetThreshold);
    r2->addWidget(quickBtn);
    r2->addStretch();
    ml->addLayout(r2);

    // 第三行：翻页 + 排序 + 导出
    QHBoxLayout *r3 = new QHBoxLayout();
    m_prevBtn = new QPushButton(QStringLiteral("◀ 上一页"), this);
    m_prevBtn->setEnabled(false); r3->addWidget(m_prevBtn);
    m_pageLabel = new QLabel(QStringLiteral("第 1/1 页，共 0 条"), this);
    r3->addWidget(m_pageLabel);
    m_nextBtn = new QPushButton(QStringLiteral("下一页 ▶"), this);
    m_nextBtn->setEnabled(false); r3->addWidget(m_nextBtn);
    r3->addWidget(new QLabel(QStringLiteral("排序:"), this));
    m_sortCombo = new QComboBox(this);
    m_sortCombo->addItem(QStringLiteral("综合排序"), 1);
    m_sortCombo->addItem(QStringLiteral("最新发布"), 2);
    m_sortCombo->addItem(QStringLiteral("价格从低到高"), 3);
    m_sortCombo->addItem(QStringLiteral("价格从高到低"), 4);
    r3->addWidget(m_sortCombo);
    connect(m_sortCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (!m_items.isEmpty() && !m_baseUrl.isEmpty()) { m_currentPage = 1; doFetchPage(); }
    });
    r3->addStretch();
    m_exportAllBtn = new QPushButton(QStringLiteral("导出全部"), this);
    r3->addWidget(m_exportAllBtn);
    ml->addLayout(r3);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(9);
    m_table->setHorizontalHeaderLabels({QStringLiteral("图片"),QStringLiteral("标题"),QStringLiteral("价格"),
        QStringLiteral("区服"),QStringLiteral("月卡到期"),QStringLiteral("剩余天数"),
        QStringLiteral("等级"),QStringLiteral("职业"),QStringLiteral("发布时间")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setDefaultSectionSize(80);
    connect(m_table, &QTableWidget::customContextMenuRequested, this, &DD373Dialog::onContextMenu);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, &DD373Dialog::onImageDoubleClicked);
    ml->addWidget(m_table, 1);

    connect(m_fetchBtn, &QPushButton::clicked, this, &DD373Dialog::onFetch);
    connect(m_prevBtn, &QPushButton::clicked, this, &DD373Dialog::onPrevPage);
    connect(m_nextBtn, &QPushButton::clicked, this, &DD373Dialog::onNextPage);
    connect(m_exportAllBtn, &QPushButton::clicked, this, &DD373Dialog::onExportAll);
}

void DD373Dialog::sendPushNotification(const QString &text)
{
    if (m_pushClient) {
        m_pushClient->sendMessage(text);
        emit statusMessage(QStringLiteral("已发送通知"));
    }
}

void DD373Dialog::onAutoRefreshTick()
{
    if (m_baseUrl.isEmpty()) return;
    QString url=m_baseUrl;
    if (url.contains(QStringLiteral("{page}"))) url.replace(QStringLiteral("{page}"), QString::number(m_currentPage));
    if (url.contains(QStringLiteral("{sort}"))) url.replace(QStringLiteral("{sort}"), QString::number(m_sortCombo->currentData().toInt()));
    emit statusMessage(QStringLiteral("自动刷新中..."));
    m_scraper->fetchPage(url);
}

void DD373Dialog::onPrevPage() { if (m_currentPage <= 1) return; m_currentPage--; doFetchPage(); }
void DD373Dialog::onNextPage() { if (m_currentPage >= m_totalPages) return; m_currentPage++; doFetchPage(); }

void DD373Dialog::onFetch()
{
    QString url = m_urlEdit->text().trimmed();
    if (url.isEmpty()) { QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("请输入 URL")); return; }
    m_baseUrl = url; m_currentPage = 1;
    QString pu = url;
    if (pu.contains(QStringLiteral("{page}"))) pu.replace(QStringLiteral("{page}"), QString::number(m_currentPage));
    if (pu.contains(QStringLiteral("{sort}"))) pu.replace(QStringLiteral("{sort}"), QString::number(m_sortCombo->currentData().toInt()));
    m_fetchBtn->setEnabled(false); m_fetchBtn->setText(QStringLiteral("抓取中..."));
    emit statusMessage(QStringLiteral("正在抓取第 1 页..."));
    m_table->setRowCount(0); m_items.clear(); m_filteredItems.clear();
    m_scraper->fetchPage(pu);
    // 如果勾选了自动刷新，启动定时器
    if (m_autoRefreshCheck->isChecked())
        m_autoRefreshTimer->start(m_refreshSpin->value() * 1000);
    else
        m_autoRefreshTimer->stop();
}

void DD373Dialog::doFetchPage()
{
    QString url = m_baseUrl;
    if (url.contains(QStringLiteral("{page}"))) url.replace(QStringLiteral("{page}"), QString::number(m_currentPage));
    if (url.contains(QStringLiteral("{sort}"))) url.replace(QStringLiteral("{sort}"), QString::number(m_sortCombo->currentData().toInt()));
    m_fetchBtn->setEnabled(false); m_fetchBtn->setText(QStringLiteral("抓取中..."));
    emit statusMessage(QStringLiteral("正在抓取第 %1 页...").arg(m_currentPage));
    m_table->setRowCount(0); m_items.clear(); m_filteredItems.clear();
    m_scraper->fetchPage(url);
}

void DD373Dialog::onItemsParsed(const QList<DD373Item> &items, int totalCount, int totalPages, int currentPage)
{
    m_fetchBtn->setEnabled(true); m_fetchBtn->setText(QStringLiteral("抓取"));
    m_items = items; m_filteredItems = items;
    m_totalCount = totalCount; m_totalPages = totalPages; m_currentPage = currentPage;
    m_pageLabel->setText(QStringLiteral("第 %1/%2 页，共 %3 条").arg(currentPage).arg(totalPages).arg(totalCount));
    m_prevBtn->setEnabled(currentPage > 1); m_nextBtn->setEnabled(currentPage < totalPages);
    emit statusMessage(QStringLiteral("加载完成 - 第 %1/%2 页，共 %3 条").arg(currentPage).arg(totalPages).arg(totalCount));
    populateTable(m_filteredItems);
    if (totalCount > m_notifyThreshold && !items.isEmpty()) {
        if (isPushDeerConfigured()) {
            QString msg = QStringLiteral("DD373 找到 %1 个商品（阈值 %2）\n第一个商品价格: ￥%3").arg(totalCount).arg(m_notifyThreshold).arg(items.first().price, 0, 'f', 2);
            sendPushNotification(msg);
        } else {
            emit statusMessage(QStringLiteral("满足通知条件（%1>%2），但未配置推送密钥").arg(totalCount).arg(m_notifyThreshold));
        }
    }
}

void DD373Dialog::onError(const QString &error)
{
    m_fetchBtn->setEnabled(true); m_fetchBtn->setText(QStringLiteral("抓取"));
    emit statusMessage(QStringLiteral("错误: ") + error);
    QMessageBox::warning(this, QStringLiteral("抓取失败"), error);
}

void DD373Dialog::populateTable(const QList<DD373Item> &items)
{
    m_imageCache.clear(); m_table->setRowCount(items.size());
    for (int i = 0; i < items.size(); ++i) {
        const DD373Item &it = items[i];
        QLabel *il = new QLabel(this);
        il->setFixedSize(70,70); il->setAlignment(Qt::AlignCenter); il->setText(QStringLiteral("加载中..."));
        il->setStyleSheet(QStringLiteral("background-color:#f0f0f0;border:1px solid #ccc;"));
        m_table->setCellWidget(i, 0, il);
        if (!it.imageUrl.isEmpty()) updateImage(i, it.imageUrl);
        QTableWidgetItem *ti = new QTableWidgetItem(it.title); ti->setToolTip(it.fullUrl); m_table->setItem(i, 1, ti);
        QTableWidgetItem *pi = new QTableWidgetItem(QStringLiteral("￥%1").arg(it.price, 0, 'f', 2));
        pi->setTextAlignment(Qt::AlignRight|Qt::AlignVCenter); m_table->setItem(i, 2, pi);
        QString ss = it.region; if (!it.server.isEmpty()) ss += " / " + it.server; m_table->setItem(i, 3, new QTableWidgetItem(ss));
        QString cs; if (!it.monthlyCardRaw.isEmpty()) cs = it.monthlyCardRaw; else if (it.monthlyCardDate.isValid()) cs = it.monthlyCardDate.toString(QStringLiteral("yyyy/M/d")); else cs = QStringLiteral("无月卡");
        m_table->setItem(i, 4, new QTableWidgetItem(cs));
        QString ds; int cd=-1; bool ud=false;
        if (it.monthlyCardDate.isValid()) cd = daysUntilDate(it.monthlyCardDate); else if (it.monthlyCardDaysLeft>=0) { cd=it.monthlyCardDaysLeft; ud=true; }
        if (cd>0) ds=QString::number(cd)+QStringLiteral(" 天"); else if (cd==0) ds=QStringLiteral("今天到期"); else ds=QStringLiteral("已过期");
        if (ud) ds += QStringLiteral(" ❓");
        QTableWidgetItem *di = new QTableWidgetItem(ds); di->setTextAlignment(Qt::AlignRight|Qt::AlignVCenter); m_table->setItem(i, 5, di);
        QTableWidgetItem *li = new QTableWidgetItem(QString::number(it.level)+QStringLiteral(" 级"));
        li->setTextAlignment(Qt::AlignRight|Qt::AlignVCenter); m_table->setItem(i, 6, li);
        m_table->setItem(i, 7, new QTableWidgetItem(it.characterClass));
        QString ps; if (it.publishDt.isValid()) ps = it.publishDt.toString(QStringLiteral("MM-dd hh:mm")); else ps = it.publishTime;
        m_table->setItem(i, 8, new QTableWidgetItem(ps));
        m_table->item(i, 1)->setData(Qt::UserRole, it.fullUrl);
    }
}

int DD373Dialog::daysUntilDate(const QDate &date) const { if (!date.isValid()) return -1; return QDate::currentDate().daysTo(date); }

void DD373Dialog::updateImage(int row, const QString &imageUrl)
{
    QNetworkAccessManager *mgr = new QNetworkAccessManager(this);
    QNetworkReply *reply = mgr->get(QNetworkRequest(QUrl(imageUrl)));
    connect(reply, &QNetworkReply::finished, this, [this, reply, row, mgr]() {
        reply->deleteLater(); mgr->deleteLater();
        if (reply->error() != QNetworkReply::NoError) { QLabel *l = qobject_cast<QLabel*>(m_table->cellWidget(row,0)); if(l) l->setText(QStringLiteral("×")); return; }
        QPixmap p; if (!p.loadFromData(reply->readAll())) { QLabel *l = qobject_cast<QLabel*>(m_table->cellWidget(row,0)); if(l) l->setText(QStringLiteral("图")); return; }
        m_imageCache[row] = p;
        QLabel *l = qobject_cast<QLabel*>(m_table->cellWidget(row,0));
        if (l) { l->setPixmap(p.scaled(70,70,Qt::KeepAspectRatio,Qt::SmoothTransformation)); l->setText(QString()); l->setCursor(Qt::PointingHandCursor); l->setToolTip(QStringLiteral("双击放大查看")); }
    });
}

void DD373Dialog::onImageDoubleClicked(int row,int col)
{
    if (col != 0 || !m_imageCache.contains(row)) return;
    QPixmap p = m_imageCache.value(row); if (p.isNull()) return;
    QDialog *dlg = new QDialog(this); dlg->setWindowTitle(QStringLiteral("商品图片")); dlg->setAttribute(Qt::WA_DeleteOnClose);
    QVBoxLayout *ly = new QVBoxLayout(dlg); ly->setContentsMargins(0,0,0,0);
    QLabel *lb = new QLabel(dlg); lb->setPixmap(p); lb->setAlignment(Qt::AlignCenter); lb->setScaledContents(true); ly->addWidget(lb);
    QSize ss = QApplication::primaryScreen()->availableSize();
    dlg->resize(qMin(p.width()+40,qMin(ss.width()-100,800)),qMin(p.height()+40,qMin(ss.height()-100,600)));
    dlg->show();
}

void DD373Dialog::onContextMenu(const QPoint &pos) { QTableWidgetItem *item = m_table->itemAt(pos); if (!item) return; m_contextMenu->exec(m_table->viewport()->mapToGlobal(pos)); }
void DD373Dialog::onOpenUrl() { auto sel = m_table->selectedItems(); if (sel.isEmpty()) return; int r = sel.first()->row(); QTableWidgetItem *ti = m_table->item(r,1); if (!ti) return; QString u = ti->data(Qt::UserRole).toString(); if (!u.isEmpty()) QDesktopServices::openUrl(QUrl(u)); }

void DD373Dialog::onExportSelected() {
    auto sel = m_table->selectedItems(); if (sel.isEmpty()) { QMessageBox::information(this, QStringLiteral("导出"), QStringLiteral("请先选择行")); return; }
    QSet<int> rows; for (auto *i : sel) rows.insert(i->row());
    QString fn = QFileDialog::getSaveFileName(this, QStringLiteral("导出选中"), QStringLiteral("dd373_selected.csv"), QStringLiteral("CSV (*.csv)"));
    if (fn.isEmpty()) return; QFile f(fn); if (!f.open(QIODevice::WriteOnly|QIODevice::Text)) { QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("无法写入")); return; }
    QTextStream out(&f); out.setEncoding(QStringConverter::Utf8); out << QChar(0xFEFF);
    out << QStringLiteral("标题,价格,区服,月卡到期,等级,职业,发布时间,URL\n");
    auto esc = [](const QString &s){ QString r=s; r.replace("\"","\"\""); return "\""+r+"\""; };
    for (int r : rows) {
        QTableWidgetItem *ti = m_table->item(r, 1); if (!ti) continue;
        const DD373Item *it = nullptr;
        for (int i=0;i<m_filteredItems.size();++i) if (m_filteredItems[i].title==ti->text()) { it=&m_filteredItems[i]; break; }
        if (!it) continue;
        QStringList fl; fl << esc(it->title) << QString::number(it->price,'f',2) << esc(it->region+"/"+it->server) << esc(it->monthlyCardRaw) << QString::number(it->level) << esc(it->characterClass) << esc(it->publishTime) << esc(it->fullUrl);
        out << fl.join(",") << "\n";
    }
    f.close(); emit statusMessage(QStringLiteral("已导出 %1 行").arg(rows.size()));
}

void DD373Dialog::onExportAll()
{
    if (m_filteredItems.isEmpty()) { QMessageBox::information(this, QStringLiteral("导出"), QStringLiteral("无数据")); return; }
    QString fn = QFileDialog::getSaveFileName(this, QStringLiteral("导出全部"), QStringLiteral("dd373_all.csv"), QStringLiteral("CSV (*.csv)"));
    if (fn.isEmpty()) return; QFile f(fn); if (!f.open(QIODevice::WriteOnly|QIODevice::Text)) { QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("无法写入")); return; }
    QTextStream out(&f); out.setEncoding(QStringConverter::Utf8); out << QChar(0xFEFF);
    out << QStringLiteral("标题,价格,区服,月卡到期,剩余天数,等级,职业,发布时间,URL\n");
    auto esc = [](const QString &s){ QString r=s; r.replace("\"","\"\""); return "\""+r+"\""; };
    for (const auto &it : m_filteredItems) {
        QStringList fl; fl << esc(it.title) << QString::number(it.price,'f',2) << esc(it.region+"/"+it.server) << esc(it.monthlyCardRaw);
        int cd=-1; if (it.monthlyCardDate.isValid()) cd=daysUntilDate(it.monthlyCardDate); else if (it.monthlyCardDaysLeft>=0) cd=it.monthlyCardDaysLeft;
        QString ds; if (cd>0) ds=QString::number(cd); else if (cd==0) ds=QStringLiteral("0"); else ds=QStringLiteral("已过期");
        if (it.monthlyCardDoubtful && cd>=0) ds+=QStringLiteral(" ?");
        fl << ds << QString::number(it.level) << esc(it.characterClass) << esc(it.publishTime) << esc(it.fullUrl);
        out << fl.join(",") << "\n";
    }
    f.close(); emit statusMessage(QStringLiteral("已导出 %1 行").arg(m_filteredItems.size()));
}

void DD373Dialog::onFilterLessThan10Days() { if (m_items.isEmpty()) return; QList<DD373Item> fi; for (const auto &it : m_items) { int d=-1; if (it.monthlyCardDate.isValid()) d=daysUntilDate(it.monthlyCardDate); else if (it.monthlyCardDaysLeft>=0) d=it.monthlyCardDaysLeft; if (d>=0 && d<10) fi.append(it); } m_filteredItems=fi; emit statusMessage(QStringLiteral("月卡到期<10天: %1 个").arg(fi.size())); populateTable(fi); }
void DD373Dialog::onFilterLessThan5Days() { if (m_items.isEmpty()) return; QList<DD373Item> fi; for (const auto &it : m_items) { int d=-1; if (it.monthlyCardDate.isValid()) d=daysUntilDate(it.monthlyCardDate); else if (it.monthlyCardDaysLeft>=0) d=it.monthlyCardDaysLeft; if (d>=0 && d<5) fi.append(it); } m_filteredItems=fi; emit statusMessage(QStringLiteral("月卡到期<5天: %1 个").arg(fi.size())); populateTable(fi); }
void DD373Dialog::onClearFilter() { m_filteredItems=m_items; emit statusMessage(QStringLiteral("共 %1 个").arg(m_items.size())); populateTable(m_filteredItems); }