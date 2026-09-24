#pragma once

#include "model/review.h"

#include <QDialog>
#include <QVector>

class QLabel;
class QPushButton;
class QTimer;

namespace ncs {

class UserClientFacade;

class ReviewDialog : public QDialog
{
    Q_OBJECT

public:
    ReviewDialog(qint64 orderId, UserClientFacade &facade,
                 QWidget *parent = nullptr);

signals:
    void reviewCompleted(const ncs::Review &review);

private:
    void selectScore(int dimension, int score);
    void refreshPreview();
    void setSubmitting(bool submitting);
    void resetAfterFailure(const QString &message);
    QString friendlyError(int code, const QString &message) const;

    qint64 orderId_ = 0;
    UserClientFacade &facade_;
    QVector<QVector<QPushButton *>> starButtons_;
    QVector<int> scores_;
    QLabel *previewLabel_ = nullptr;
    QLabel *stateLabel_ = nullptr;
    QPushButton *submitButton_ = nullptr;
    QPushButton *laterButton_ = nullptr;
    QTimer *timeoutTimer_ = nullptr;
    bool submitting_ = false;
};

}
