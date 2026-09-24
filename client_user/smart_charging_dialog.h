#pragma once

#include "model/charger.h"
#include "model/station.h"
#include "util/smart_charging_planner.h"

#include <QDialog>
#include <QVector>

class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QSpinBox;

namespace ncs {

class SmartChargingDialog : public QDialog
{
    Q_OBJECT

public:
    SmartChargingDialog(const Station &station, const Charger &charger,
                        double userBalance, QWidget *parent = nullptr);

    bool hasSelectedPlan() const;
    SmartChargingPlan selectedPlan() const;
    SmartChargingInput selectedInput() const;

private:
    void generatePlans();
    void selectMode(SmartChargingMode mode);
    void renderPlans();
    const SmartChargingPlan *planForMode(SmartChargingMode mode) const;

    Station station_;
    Charger charger_;
    double userBalance_ = 0.0;
    SmartChargingInput input_;
    SmartChargingResult result_;
    SmartChargingMode selectedMode_ = SmartChargingMode::Balanced;
    QDoubleSpinBox *currentSoc_ = nullptr;
    QDoubleSpinBox *targetSoc_ = nullptr;
    QDoubleSpinBox *batteryCapacity_ = nullptr;
    QDoubleSpinBox *reserveBalance_ = nullptr;
    QSpinBox *availableMinutes_ = nullptr;
    QVector<QPushButton *> planButtons_;
    QLabel *recommendationTitle_ = nullptr;
    QLabel *recommendationNumber_ = nullptr;
    QLabel *recommendationMetrics_ = nullptr;
    QLabel *constraintLabel_ = nullptr;
    QLabel *explanationLabel_ = nullptr;
    QLabel *stateLabel_ = nullptr;
    QPushButton *applyButton_ = nullptr;
};

}
