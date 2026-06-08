#ifndef DD373ITEM_H
#define DD373ITEM_H

#include <QString>
#include <QDate>
#include <QUrl>

struct DD373Item {
    QString title;           // 商品标题
    QString detailUrl;       // 详情页相对路径
    QString fullUrl;         // 完整 URL
    QString imageUrl;        // 图片 URL
    double price = 0.0;      // 价格
    QString region;          // 区（台服/韩服）
    QString server;          // 服（獨眼巨人等）
    QString accountType;     // 账号类型
    bool hasMonthlyCard = false;  // 是否有月卡
    QString monthlyCardRaw;  // 月卡到期原始文本
    QDate monthlyCardDate;   // 解析后的月卡到期日期（无效日期表示解析失败）
    int monthlyCardDaysLeft = -1; // 月卡剩余天数（-1 表示未知或已到期）
    bool monthlyCardDoubtful = false; // 月卡时间存疑（需要根据发布时间判断）
    QString characterClass;  // 职业
    int level = 0;           // 等级
    QString publishTime;     // 发布时间原始字符串
    QDateTime publishDt;     // 解析后的发布时间
    bool valid = false;      // 是否有效
};

#endif // DD373ITEM_H