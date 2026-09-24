#include "user_main_window.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace ncs {

void UserMainWindow::showRecommendations(
    const QVector<StationRecommendation> &recommendations)
{
    while (recommendationLayout_->count() > 0) {
        QLayoutItem *item = recommendationLayout_->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    if (recommendations.isEmpty()) {
        auto *empty = new QLabel(QStringLiteral("暂无可推荐电站"), recommendationPanel_);
        empty->setObjectName(QStringLiteral("mutedLabel"));
        empty->setWordWrap(true);
        recommendationLayout_->addWidget(empty);
        return;
    }

    int rank = 1;
    for (const StationRecommendation &item : recommendations) {
        auto *button = new QPushButton(recommendationPanel_);
        button->setObjectName(QStringLiteral("recommendationCard"));
        button->setProperty("variant", QStringLiteral("recommendation-card"));
        button->setMinimumHeight(72);
        // 长文本不得撑大整页最小宽度（整页滚动下会被左右裁切）
        button->setMinimumWidth(0);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        // 文本必须落在按钮宽度内：站名限长、理由最多取 2 条，全文见 Tooltip
        QString name = item.station.name;
        if (name.size() > 10) name = name.left(10) + QStringLiteral("…");
        button->setText(QStringLiteral(
            "No.%1  %2  ·  %3分\n%4 / 空闲 %5/%6 / ¥%7/kWh")
                .arg(rank++)
                .arg(name)
                .arg(item.score, 0, 'f', 0)
                .arg(item.reasons.mid(0, 2).join(QStringLiteral(" · ")))
                .arg(item.station.idleSlots)
                .arg(item.station.chargerCount > 0
                         ? item.station.chargerCount : item.station.totalSlots)
                .arg(item.station.price, 0, 'f', 2));
        button->setToolTip(item.reasons.join(QStringLiteral("\n")));
        connect(button, &QPushButton::clicked, this,
                [this, stationId = item.station.id] {
                    requestStationOpen(stationId);
                });
        recommendationLayout_->addWidget(button);
    }
}

}
