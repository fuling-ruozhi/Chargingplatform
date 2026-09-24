#include "user_main_window.h"

#include "station_sorting.h"

#include <QFrame>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace ncs {

void UserMainWindow::renderStations()
{
    const auto mode = static_cast<StationSortMode>(sortBox_->currentData().toInt());
    const QVector<Station> displayed = sortStations(stations_, mode);
    availabilityLabels_.clear();
    stationButtons_.clear();
    availabilityPending_.clear();
    while (stationLayout_->count() > 0) {
        QLayoutItem *item = stationLayout_->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    if (displayed.isEmpty()) {
        auto *empty = new QLabel(QStringLiteral("附近暂无可展示的充电站"), this);
        empty->setObjectName(QStringLiteral("mutedLabel"));
        empty->setAlignment(Qt::AlignCenter);
        stationLayout_->addStretch();
        stationLayout_->addWidget(empty);
        stationLayout_->addStretch();
        setState(QStringLiteral("暂无电站"));
        return;
    }

    for (const Station &station : displayed) {
        auto *card = new QFrame(this);
        card->setObjectName(QStringLiteral("stationCard"));
        auto *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(14, 13, 14, 13);
        cardLayout->setSpacing(5);
        auto *name = new QLabel(station.name, card);
        name->setObjectName(QStringLiteral("sectionTitle"));
        auto *address = new QLabel(station.address, card);
        address->setObjectName(QStringLiteral("mutedLabel"));
        address->setWordWrap(true);
        auto *metaRow = new QHBoxLayout;
        auto *price = new QLabel(QStringLiteral("%1 元/kWh")
                                     .arg(station.price, 0, 'f', 2), card);
        price->setStyleSheet(QStringLiteral("color:#2563eb;font-weight:700"));
        auto *available = new QLabel(
            QStringLiteral("可用 %1 / %2  ·  %3 km")
                .arg(station.idleSlots).arg(station.totalSlots)
                .arg(station.distanceKm, 0, 'f', 2), card);
        available->setObjectName(QStringLiteral("mutedLabel"));
        auto *rating = new QLabel(station.reviewCount > 0
                                      ? QStringLiteral("★ %1 · %2人评价")
                                            .arg(station.averageScore, 0, 'f', 1)
                                            .arg(station.reviewCount)
                                      : QStringLiteral("暂无评价"), card);
        rating->setObjectName(QStringLiteral("mutedLabel"));
        metaRow->addWidget(price);
        metaRow->addStretch();
        metaRow->addWidget(available);
        cardLayout->addWidget(rating);
        if (mode == StationSortMode::Recommendation) {
            auto *recommend = new QLabel(QStringLiteral("推荐 %1")
                                              .arg(station.recommendScore, 0, 'f', 1), card);
            recommend->setObjectName(QStringLiteral("mutedLabel"));
            cardLayout->addWidget(recommend);
        }
        auto *button = new QPushButton(QStringLiteral("查看电桩"), card);
        button->setObjectName(QStringLiteral("secondaryButton"));
        connect(button, &QPushButton::clicked, this,
                [this, station] { requestStationOpen(station.id); });
        cardLayout->addWidget(name);
        cardLayout->addWidget(address);
        cardLayout->addLayout(metaRow);
        cardLayout->addWidget(button);
        stationLayout_->addWidget(card);
        availabilityLabels_.insert(station.id, available);
        stationButtons_.insert(station.id, button);
    }
    stationLayout_->addStretch();
    const QString modeText = mode == StationSortMode::Rating ? QStringLiteral("评分")
        : mode == StationSortMode::Recommendation ? QStringLiteral("推荐") : QStringLiteral("距离");
    setState(QStringLiteral("共 %1 个电站，已按%2排序").arg(displayed.size()).arg(modeText));
}

}
