#include "smart_charging_dialog.h"

#include "config/charge_config.h"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QStyle>
#include <QVBoxLayout>

namespace ncs {
namespace {

QString modeKey(SmartChargingMode mode)
{
    switch (mode) {
    case SmartChargingMode::Fastest: return QStringLiteral("fastest");
    case SmartChargingMode::Balanced: return QStringLiteral("balanced");
    case SmartChargingMode::Economy: return QStringLiteral("economy");
    }
    return QStringLiteral("unknown");
}

void refreshStyle(QWidget *widget)
{
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
}

}  // namespace

SmartChargingDialog::SmartChargingDialog(const Station &station,
                                         const Charger &charger,
                                         double userBalance, QWidget *parent)
    : QDialog(parent), station_(station), charger_(charger),
      userBalance_(userBalance)
{
    setWindowTitle(QStringLiteral("智慧充电规划"));
    setFixedSize(440, 760);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(8);
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    auto *page = new QWidget(scroll);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(9);

    auto *hero = new QFrame(page);
    hero->setObjectName(QStringLiteral("gradientHero"));
    auto *heroLayout = new QVBoxLayout(hero);
    heroLayout->setContentsMargins(18, 15, 18, 15);
    heroLayout->setSpacing(3);
    auto *title = new QLabel(QStringLiteral("智慧充电"), hero);
    title->setObjectName(QStringLiteral("heroTitle"));
    auto *chargerLabel = new QLabel(
        QStringLiteral("%1 · %2").arg(station_.name, charger_.code), hero);
    chargerLabel->setObjectName(QStringLiteral("heroSubtitle"));
    auto *facts = new QLabel(
        QStringLiteral("真实桩功率 %1 kW  ·  实时电价 ¥%2/kWh  ·  余额 ¥%3")
            .arg(charger_.powerKw, 0, 'f', 1)
            .arg(station_.price, 0, 'f', 2)
            .arg(userBalance_, 0, 'f', 2), hero);
    facts->setObjectName(QStringLiteral("heroSubtitle"));
    facts->setWordWrap(true);
    heroLayout->addWidget(title);
    heroLayout->addWidget(chargerLabel);
    heroLayout->addWidget(facts);
    layout->addWidget(hero);

    auto *inputCard = new QFrame(page);
    inputCard->setObjectName(QStringLiteral("infoCard"));
    auto *inputLayout = new QVBoxLayout(inputCard);
    inputLayout->setContentsMargins(15, 13, 15, 13);
    inputLayout->setSpacing(8);
    auto *inputTitle = new QLabel(QStringLiteral("你的出行计划"), inputCard);
    inputTitle->setObjectName(QStringLiteral("sectionTitle"));
    inputLayout->addWidget(inputTitle);
    auto *form = new QFormLayout;
    form->setHorizontalSpacing(10);
    form->setVerticalSpacing(7);

    auto makeDoubleInput = [inputCard](const QString &name, double minimum,
                                       double maximum, double value,
                                       const QString &suffix) {
        auto *input = new QDoubleSpinBox(inputCard);
        input->setObjectName(name);
        input->setRange(minimum, maximum);
        input->setDecimals(1);
        input->setSingleStep(1.0);
        input->setValue(value);
        input->setSuffix(suffix);
        return input;
    };
    currentSoc_ = makeDoubleInput(QStringLiteral("currentSocInput"), 0.0, 100.0,
                                  30.0, QStringLiteral(" %"));
    targetSoc_ = makeDoubleInput(QStringLiteral("targetSocInput"), 0.0, 100.0,
                                 80.0, QStringLiteral(" %"));
    batteryCapacity_ = makeDoubleInput(QStringLiteral("batteryCapacityInput"),
                                       1.0, 200.0,
                                       ChargeConfig::batteryCapacityKwh(),
                                       QStringLiteral(" kWh"));
    reserveBalance_ = makeDoubleInput(QStringLiteral("reserveBalanceInput"),
                                      0.0, qMax(0.0, userBalance_),
                                      qMin(10.0, qMax(0.0, userBalance_)),
                                      QStringLiteral(" 元"));
    availableMinutes_ = new QSpinBox(inputCard);
    availableMinutes_->setObjectName(QStringLiteral("availableMinutesInput"));
    availableMinutes_->setRange(0, 1440);
    availableMinutes_->setValue(60);
    availableMinutes_->setSuffix(QStringLiteral(" 分钟后"));
    form->addRow(QStringLiteral("当前电量"), currentSoc_);
    form->addRow(QStringLiteral("目标电量"), targetSoc_);
    form->addRow(QStringLiteral("电池容量"), batteryCapacity_);
    form->addRow(QStringLiteral("希望保留余额"), reserveBalance_);
    form->addRow(QStringLiteral("预计离开"), availableMinutes_);
    inputLayout->addLayout(form);
    auto *generate = new QPushButton(QStringLiteral("生成智慧方案"), inputCard);
    generate->setObjectName(QStringLiteral("generateSmartPlanButton"));
    inputLayout->addWidget(generate);
    layout->addWidget(inputCard);

    stateLabel_ = new QLabel(page);
    stateLabel_->setWordWrap(true);
    layout->addWidget(stateLabel_);
    auto *plansTitle = new QLabel(QStringLiteral("三个可行方案"), page);
    plansTitle->setObjectName(QStringLiteral("sectionTitle"));
    layout->addWidget(plansTitle);
    for (SmartChargingMode mode : {SmartChargingMode::Fastest,
                                   SmartChargingMode::Balanced,
                                   SmartChargingMode::Economy}) {
        auto *button = new QPushButton(page);
        button->setObjectName(modeKey(mode) + QStringLiteral("PlanCard"));
        button->setProperty("variant", QStringLiteral("plan-card"));
        button->setProperty("mode", modeKey(mode));
        button->setCheckable(true);
        button->setMinimumHeight(84);
        connect(button, &QPushButton::clicked, this,
                [this, mode] { selectMode(mode); });
        planButtons_.append(button);
        layout->addWidget(button);
    }

    auto *recommendation = new QFrame(page);
    recommendation->setObjectName(QStringLiteral("featuredCard"));
    auto *recommendationLayout = new QVBoxLayout(recommendation);
    recommendationLayout->setContentsMargins(16, 14, 16, 14);
    recommendationLayout->setSpacing(5);
    recommendationTitle_ = new QLabel(recommendation);
    recommendationTitle_->setObjectName(QStringLiteral("heroSubtitle"));
    recommendationNumber_ = new QLabel(recommendation);
    recommendationNumber_->setObjectName(QStringLiteral("heroTitle"));
    recommendationMetrics_ = new QLabel(recommendation);
    recommendationMetrics_->setObjectName(QStringLiteral("heroSubtitle"));
    recommendationMetrics_->setWordWrap(true);
    constraintLabel_ = new QLabel(recommendation);
    constraintLabel_->setObjectName(QStringLiteral("smartConstraintBadge"));
    constraintLabel_->setWordWrap(true);
    explanationLabel_ = new QLabel(recommendation);
    explanationLabel_->setObjectName(QStringLiteral("heroSubtitle"));
    explanationLabel_->setWordWrap(true);
    recommendationLayout->addWidget(recommendationTitle_);
    recommendationLayout->addWidget(recommendationNumber_);
    recommendationLayout->addWidget(recommendationMetrics_);
    recommendationLayout->addWidget(constraintLabel_);
    recommendationLayout->addWidget(explanationLabel_);
    layout->addWidget(recommendation);
    layout->addStretch();
    scroll->setWidget(page);
    root->addWidget(scroll, 1);

    applyButton_ = new QPushButton(QStringLiteral("采用该方案并预约"), this);
    applyButton_->setObjectName(QStringLiteral("applySmartPlanButton"));
    auto *closeButton = new QPushButton(QStringLiteral("暂不采用"), this);
    closeButton->setObjectName(QStringLiteral("ghostButton"));
    root->addWidget(applyButton_);
    root->addWidget(closeButton);
    connect(generate, &QPushButton::clicked,
            this, &SmartChargingDialog::generatePlans);
    connect(applyButton_, &QPushButton::clicked, this, &QDialog::accept);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::reject);
    generatePlans();
}

bool SmartChargingDialog::hasSelectedPlan() const
{
    return result_.success && planForMode(selectedMode_) != nullptr;
}

SmartChargingPlan SmartChargingDialog::selectedPlan() const
{
    const SmartChargingPlan *plan = planForMode(selectedMode_);
    return plan ? *plan : SmartChargingPlan();
}

SmartChargingInput SmartChargingDialog::selectedInput() const
{
    return input_;
}

void SmartChargingDialog::generatePlans()
{
    input_.currentSocPercent = currentSoc_->value();
    input_.targetSocPercent = targetSoc_->value();
    input_.batteryCapacityKwh = batteryCapacity_->value();
    input_.chargerPowerKw = charger_.powerKw;
    input_.pricePerKwh = station_.price;
    input_.userBalance = userBalance_;
    input_.reserveBalance = reserveBalance_->value();
    input_.availableMinutes = availableMinutes_->value();
    result_ = SmartChargingPlanner::plan(input_);
    if (!result_.success) {
        stateLabel_->setObjectName(QStringLiteral("statusError"));
        stateLabel_->setText(result_.error);
        applyButton_->setEnabled(false);
        for (QPushButton *button : planButtons_) button->setEnabled(false);
        refreshStyle(stateLabel_);
        return;
    }
    selectedMode_ = result_.recommendedMode;
    stateLabel_->setObjectName(QStringLiteral("statusSuccess"));
    stateLabel_->setText(QStringLiteral("已根据真实余额、桩功率和电价生成离线可解释方案"));
    refreshStyle(stateLabel_);
    for (QPushButton *button : planButtons_) button->setEnabled(true);
    renderPlans();
}

void SmartChargingDialog::selectMode(SmartChargingMode mode)
{
    selectedMode_ = mode;
    renderPlans();
}

void SmartChargingDialog::renderPlans()
{
    for (QPushButton *button : planButtons_) {
        SmartChargingMode mode = SmartChargingMode::Economy;
        if (button->property("mode") == QStringLiteral("fastest")) {
            mode = SmartChargingMode::Fastest;
        } else if (button->property("mode") == QStringLiteral("balanced")) {
            mode = SmartChargingMode::Balanced;
        }
        const SmartChargingPlan *plan = planForMode(mode);
        if (!plan) continue;
        const bool recommended = mode == result_.recommendedMode;
        button->setChecked(mode == selectedMode_);
        button->setProperty("recommended", recommended);
        button->setText(QStringLiteral("%1%2\n建议 %3%  ·  %4 kWh  ·  %5 分钟  ·  ¥%6")
                            .arg(plan->title,
                                 recommended ? QStringLiteral("  推荐") : QString())
                            .arg(plan->recommendedTargetSoc, 0, 'f', 1)
                            .arg(plan->energyKwh, 0, 'f', 1)
                            .arg(plan->estimatedMinutes)
                            .arg(plan->estimatedAmount, 0, 'f', 2));
        refreshStyle(button);
    }

    const SmartChargingPlan *plan = planForMode(selectedMode_);
    if (!plan) return;
    recommendationTitle_->setText(
        selectedMode_ == result_.recommendedMode
            ? QStringLiteral("系统推荐 · %1").arg(plan->title)
            : QStringLiteral("当前选择 · %1").arg(plan->title));
    recommendationNumber_->setText(
        QStringLiteral("建议充至 %1%").arg(plan->recommendedTargetSoc, 0, 'f', 1));
    recommendationMetrics_->setText(
        QStringLiteral("补能 %1 kWh  ·  现实预计 %2 分钟  ·  ¥%3  ·  充后余额 ¥%4")
            .arg(plan->energyKwh, 0, 'f', 1)
            .arg(plan->estimatedMinutes)
            .arg(plan->estimatedAmount, 0, 'f', 2)
            .arg(plan->balanceAfter, 0, 'f', 2));
    QStringList factors;
    for (SmartChargingLimitingFactor factor : plan->limitingFactors) {
        factors.append(SmartChargingPlanner::limitingFactorText(factor));
    }
    constraintLabel_->setText(factors.join(QStringLiteral(" · ")));
    explanationLabel_->setText(plan->summary);
    applyButton_->setEnabled(plan->energyKwh > 0.000001);
}

const SmartChargingPlan *SmartChargingDialog::planForMode(
    SmartChargingMode mode) const
{
    for (const SmartChargingPlan &plan : result_.plans) {
        if (plan.mode == mode) return &plan;
    }
    return nullptr;
}

}
