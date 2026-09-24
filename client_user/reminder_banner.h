#pragma once

#include "model/user_preference.h"

#include <QFrame>

class QLabel;
class QPushButton;

namespace ncs {

class ReminderBanner : public QFrame
{
    Q_OBJECT

public:
    explicit ReminderBanner(QWidget *parent = nullptr);
    void showMatches(const QVector<ReminderMatch> &matches);

private:
    QLabel *messageLabel_ = nullptr;
    QPushButton *closeButton_ = nullptr;
};

}  // namespace ncs
