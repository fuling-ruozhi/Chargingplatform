#pragma once

#include "model/vehicle_profile.h"

#include <QDialog>

class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QTimeEdit;
class QTimer;

namespace ncs {

class UserClientFacade;

// UC-EXT-SC-01 车辆档案：电池容量 / 目标 SoC / 余额保留值 /
// 常用离开时间 / 充电模式（省钱·快速·均衡）的一次性设置页。
class VehicleProfileDialog : public QDialog
{
    Q_OBJECT
public:
    explicit VehicleProfileDialog(UserClientFacade &facade, QWidget *parent = nullptr);

private:
    void applyProfile(const VehicleProfile &profile);
    VehicleProfile collectProfile() const;
    void beginRequest(const QString &route, const QString &message);
    void finishRequest(const QString &message, bool error);

    UserClientFacade &facade_;
    QSpinBox *capacity_ = nullptr;
    QSpinBox *targetSoc_ = nullptr;
    QDoubleSpinBox *reserve_ = nullptr;
    QTimeEdit *leaveTime_ = nullptr;
    QRadioButton *economy_ = nullptr;
    QRadioButton *balanced_ = nullptr;
    QRadioButton *fast_ = nullptr;
    QLabel *stateLabel_ = nullptr;
    QPushButton *saveButton_ = nullptr;
    QTimer *requestTimer_ = nullptr;
    QString pendingRoute_;
};

}
