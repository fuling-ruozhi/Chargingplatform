#include "review_dialog.h"
#include "station_detail_dialog.h"
#include "service/user_client_facade.h"

#include <QApplication>
#include <QLabel>
#include <QPushButton>

namespace {

QPushButton *button(QWidget &widget, const QString &objectName)
{
    for (QPushButton *candidate : widget.findChildren<QPushButton *>()) {
        if (candidate->objectName() == objectName) return candidate;
    }
    return nullptr;
}

QVector<QPushButton *> stars(QWidget &widget)
{
    QVector<QPushButton *> result;
    for (QPushButton *candidate : widget.findChildren<QPushButton *>()) {
        if (candidate->objectName() == QStringLiteral("reviewStarButton")) {
            result.append(candidate);
        }
    }
    return result;
}

}

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    ncs::UserClientFacade facade;
    ncs::ReviewDialog dialog(42, facade);
    dialog.show();

    auto *submit = button(dialog, QStringLiteral("reviewSubmitButton"));
    const QVector<QPushButton *> starButtons = stars(dialog);
    if (!submit || submit->isEnabled() || starButtons.size() != 20) return 1;

    const QVector<int> scores{5, 4, 5, 3};
    for (int dimension = 0; dimension < scores.size(); ++dimension) {
        starButtons.at(dimension * 5 + scores.at(dimension) - 1)->click();
    }
    auto *preview = dialog.findChild<QLabel *>(QStringLiteral("reviewPreviewLabel"));
    if (!preview || !preview->text().contains(QStringLiteral("4.25"))
        || !submit->isEnabled()) return 2;

    int failedRequests = 0;
    QObject::connect(&facade, &ncs::UserClientFacade::requestFailed,
                     [&](const QString &route, int, const QString &) {
                         if (route == QStringLiteral("review.submit")) ++failedRequests;
                     });
    submit->click();
    QApplication::processEvents();
    if (failedRequests != 1 || !submit->isEnabled()) return 3;

    ncs::StationDetail emptyDetail;
    emptyDetail.station.id = 1;
    emptyDetail.station.name = QStringLiteral("测试电站");
    emptyDetail.station.address = QStringLiteral("测试地址");
    ncs::StationDetailDialog emptyStation(emptyDetail, 1, facade);
    bool hasEmptyText = false;
    for (QLabel *label : emptyStation.findChildren<QLabel *>()) {
        if (label->text().contains(QStringLiteral("暂无用户评价"))) hasEmptyText = true;
    }
    if (!hasEmptyText) return 4;

    ncs::StationDetail ratedDetail = emptyDetail;
    ratedDetail.ratingSummary.reviewCount = 20;
    ratedDetail.ratingSummary.averageScore = 4.5;
    ratedDetail.ratingSummary.environmentAverage = 4.7;
    ratedDetail.ratingSummary.queueAverage = 4.1;
    ratedDetail.ratingSummary.equipmentAverage = 4.6;
    ratedDetail.ratingSummary.parkingAverage = 4.4;
    ncs::StationDetailDialog ratedStation(ratedDetail, 1, facade);
    bool hasRatedText = false;
    for (QLabel *label : ratedStation.findChildren<QLabel *>()) {
        if (label->text().contains(QStringLiteral("4.5"))
            && label->text().contains(QStringLiteral("20"))) hasRatedText = true;
    }
    if (!hasRatedText) return 5;
    return 0;
}
