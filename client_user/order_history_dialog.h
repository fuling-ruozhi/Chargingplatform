#pragma once

#include "model/charging_record.h"
#include "model/review.h"

#include <QDialog>
#include <QHash>
#include <QQueue>
#include <QSet>
#include <QVector>

class QLabel;
class QPushButton;
class QTimer;
class QVBoxLayout;

namespace ncs {

class UserClientFacade;

class OrderHistoryDialog : public QDialog
{
    Q_OBJECT
public:
    explicit OrderHistoryDialog(UserClientFacade &facade,
                                QWidget *parent = nullptr);

private:
    void render(const QVector<ChargingRecord> &records);
    void requestDetail(qint64 orderId);
    void showReceipt(const ChargingRecord &record);
    void requestNextReview();
    void openReview(qint64 orderId);
    void updateReviewState(qint64 orderId, bool hasReview, const Review &review);
    void setState(const QString &message, bool error = false);
    void setButtonsEnabled(bool enabled);

    UserClientFacade &facade_;
    QLabel *stateLabel_ = nullptr;
    QVBoxLayout *ordersLayout_ = nullptr;
    QTimer *timer_ = nullptr;
    QTimer *reviewTimer_ = nullptr;
    QVector<QPushButton *> detailButtons_;
    QHash<qint64, QLabel *> reviewLabels_;
    QHash<qint64, QPushButton *> reviewButtons_;
    QQueue<qint64> reviewQueue_;
    QSet<qint64> reviewKnown_;
    QHash<qint64, Review> reviews_;
    qint64 pendingOrderId_ = 0;
    qint64 pendingReviewOrderId_ = 0;
};

}
