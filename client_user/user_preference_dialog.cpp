#include "user_preference_dialog.h"

#include "model/charger.h"
#include "service/user_client_facade.h"
#include "user_messages.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QStyle>
#include <QTimeEdit>
#include <QTimer>
#include <QVBoxLayout>

namespace ncs {

namespace {

QTime timeFromText(const QString &value, const QTime &fallback)
{
    const QTime result = QTime::fromString(value, QStringLiteral("HH:mm"));
    return result.isValid() ? result : fallback;
}

QLabel *addLabel(QFormLayout *form, const QString &text)
{
    auto *label = new QLabel(text, form->parentWidget());
    label->setObjectName(QStringLiteral("mutedLabel"));
    return label;
}

}  // namespace

UserPreferenceDialog::UserPreferenceDialog(UserClientFacade &facade,
                                           bool hasCurrentLocation,
                                           double currentLongitude,
                                           double currentLatitude,
                                           const QVector<Station> &knownStations,
                                           QWidget *parent)
    : QDialog(parent), facade_(facade), hasCurrentLocation_(hasCurrentLocation),
      currentLongitude_(currentLongitude), currentLatitude_(currentLatitude),
      knownStations_(knownStations), enabledCheck_(new QCheckBox(this)),
      homeStatusLabel_(new QLabel(this)), favoriteCountLabel_(new QLabel(this)),
      stateLabel_(new QLabel(this)), setHomeButton_(new QPushButton(this)),
      clearHomeButton_(new QPushButton(this)), radiusSpin_(new QDoubleSpinBox(this)),
      slowCheck_(new QCheckBox(this)), fastCheck_(new QCheckBox(this)),
      reminderStartEdit_(new QTimeEdit(this)), reminderEndEdit_(new QTimeEdit(this)),
      minIdleSpin_(new QSpinBox(this)), dndStartEdit_(new QTimeEdit(this)),
      dndEndEdit_(new QTimeEdit(this)), saveButton_(new QPushButton(QStringLiteral("保存设置"), this))
{
    setWindowTitle(QStringLiteral("提醒偏好"));
    setMinimumSize(430, 650);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(10);
    auto *title = new QLabel(QStringLiteral("提醒偏好设置"), this);
    title->setObjectName(QStringLiteral("pageTitle"));
    layout->addWidget(title);

    enabledCheck_->setText(QStringLiteral("启用提醒"));
    enabledCheck_->setObjectName(QStringLiteral("preferenceEnabled"));
    enabledCheck_->setChecked(true);
    layout->addWidget(enabledCheck_);
    auto *form = new QFormLayout;
    form->setHorizontalSpacing(14);
    form->setVerticalSpacing(9);

    auto *homeRow = new QHBoxLayout;
    homeStatusLabel_->setObjectName(QStringLiteral("mutedLabel"));
    setHomeButton_->setText(QStringLiteral("将当前位置设为家"));
    setHomeButton_->setObjectName(QStringLiteral("secondaryButton"));
    clearHomeButton_->setText(QStringLiteral("清除家位置"));
    clearHomeButton_->setObjectName(QStringLiteral("ghostButton"));
    homeRow->addWidget(homeStatusLabel_, 1);
    homeRow->addWidget(setHomeButton_);
    homeRow->addWidget(clearHomeButton_);
    form->addRow(QStringLiteral("家位置"), homeRow);

    radiusSpin_->setRange(0.1, 100.0);
    radiusSpin_->setObjectName(QStringLiteral("homeRadius"));
    radiusSpin_->setSingleStep(0.5);
    radiusSpin_->setDecimals(1);
    radiusSpin_->setSuffix(QStringLiteral(" km"));
    radiusSpin_->setValue(3.0);
    form->addRow(QStringLiteral("家附近半径"), radiusSpin_);

    slowCheck_->setText(chargerTypeText(static_cast<int>(ChargerType::Slow)));
    slowCheck_->setObjectName(QStringLiteral("slowChargerType"));
    fastCheck_->setText(chargerTypeText(static_cast<int>(ChargerType::Fast)));
    fastCheck_->setObjectName(QStringLiteral("fastChargerType"));
    auto *types = new QHBoxLayout;
    types->addWidget(slowCheck_);
    types->addWidget(fastCheck_);
    types->addStretch();
    form->addRow(QStringLiteral("桩类型"), types);
    form->addRow(addLabel(form, QStringLiteral("未选择时匹配全部桩类型")));

    reminderStartEdit_->setDisplayFormat(QStringLiteral("HH:mm"));
    reminderStartEdit_->setObjectName(QStringLiteral("reminderStart"));
    reminderEndEdit_->setDisplayFormat(QStringLiteral("HH:mm"));
    reminderEndEdit_->setObjectName(QStringLiteral("reminderEnd"));
    reminderStartEdit_->setTime(QTime(8, 0));
    reminderEndEdit_->setTime(QTime(22, 0));
    auto *reminderTimes = new QHBoxLayout;
    reminderTimes->addWidget(reminderStartEdit_);
    reminderTimes->addWidget(new QLabel(QStringLiteral("至"), this));
    reminderTimes->addWidget(reminderEndEdit_);
    form->addRow(QStringLiteral("提醒时段"), reminderTimes);

    minIdleSpin_->setRange(1, 100);
    minIdleSpin_->setObjectName(QStringLiteral("minIdleChargers"));
    minIdleSpin_->setValue(1);
    form->addRow(QStringLiteral("最低空闲数"), minIdleSpin_);
    form->addRow(addLabel(form, QStringLiteral("达到至少 N 个符合类型的空闲桩时提醒")));

    dndStartEdit_->setDisplayFormat(QStringLiteral("HH:mm"));
    dndStartEdit_->setObjectName(QStringLiteral("dndStart"));
    dndEndEdit_->setDisplayFormat(QStringLiteral("HH:mm"));
    dndEndEdit_->setObjectName(QStringLiteral("dndEnd"));
    dndStartEdit_->setTime(QTime(22, 0));
    dndEndEdit_->setTime(QTime(7, 0));
    auto *dndTimes = new QHBoxLayout;
    dndTimes->addWidget(dndStartEdit_);
    dndTimes->addWidget(new QLabel(QStringLiteral("至"), this));
    dndTimes->addWidget(dndEndEdit_);
    form->addRow(QStringLiteral("勿扰时段"), dndTimes);
    form->addRow(addLabel(form, QStringLiteral("勿扰时段优先于提醒时段")));
    layout->addLayout(form);

    auto *favoriteRow = new QHBoxLayout;
    favoriteCountLabel_->setObjectName(QStringLiteral("mutedLabel"));
    auto *favoriteButton = new QPushButton(QStringLiteral("查看关注站"), this);
    favoriteButton->setObjectName(QStringLiteral("secondaryButton"));
    favoriteRow->addWidget(favoriteCountLabel_, 1);
    favoriteRow->addWidget(favoriteButton);
    layout->addLayout(favoriteRow);
    stateLabel_->setObjectName(QStringLiteral("statusLoading"));
    stateLabel_->setWordWrap(true);
    layout->addWidget(stateLabel_);
    layout->addStretch();
    saveButton_->setObjectName(QStringLiteral("reviewSubmitButton"));
    layout->addWidget(saveButton_);

    connect(setHomeButton_, &QPushButton::clicked,
            this, &UserPreferenceDialog::setHomeFromCurrentLocation);
    connect(clearHomeButton_, &QPushButton::clicked,
            this, &UserPreferenceDialog::clearHomeLocation);
    connect(saveButton_, &QPushButton::clicked, this, &UserPreferenceDialog::save);
    connect(favoriteButton, &QPushButton::clicked,
            this, &UserPreferenceDialog::showFavorites);
    connect(&facade_, &UserClientFacade::preferenceReceived, this,
            &UserPreferenceDialog::loadPreference);
    connect(&facade_, &UserClientFacade::preferenceUpdated, this,
            [this](const UserPreference &value) {
                saving_ = false;
                saveButton_->setEnabled(true);
                preference_ = value;
                homeLocationDirty_ = false;
                loadPreference(value);
                showMessage(QStringLiteral("提醒偏好已保存"));
                emit preferenceSaved(value);
            });
    connect(&facade_, &UserClientFacade::favoriteStationsReceived, this,
            [this](const QVector<qint64> &ids) {
                favoriteIds_ = ids;
                favoriteCountLabel_->setText(QStringLiteral("已关注 %1 个站点").arg(ids.size()));
            });
    connect(&facade_, &UserClientFacade::favoriteStationChanged, this,
            [this](qint64 stationId, bool added) {
                favoriteIds_.removeAll(stationId);
                if (added) favoriteIds_.append(stationId);
                favoriteCountLabel_->setText(QStringLiteral("已关注 %1 个站点")
                                                 .arg(favoriteIds_.size()));
            });
    connect(&facade_, &UserClientFacade::requestFailed, this,
            [this](const QString &route, int, const QString &message) {
                if (route == QStringLiteral("preference.update") && saving_) {
                    saving_ = false;
                    saveButton_->setEnabled(true);
                    showMessage(userFacingError(message), true);
                }
            });
    refreshHomeLocationUi();
    favoriteCountLabel_->setText(QStringLiteral("已关注 0 个站点"));
    facade_.requestPreference();
    facade_.requestFavoriteStations();
}

void UserPreferenceDialog::loadPreference(const UserPreference &value)
{
    preference_ = value;
    if (!homeLocationDirty_) {
        homeSet_ = value.hasHomeLocation;
        homeLatitude_ = value.homeLatitude;
        homeLongitude_ = value.homeLongitude;
    }
    enabledCheck_->setChecked(value.enabled);
    radiusSpin_->setValue(value.homeRadiusKm);
    slowCheck_->setChecked(value.preferredChargerTypes.contains(static_cast<int>(ChargerType::Slow)));
    fastCheck_->setChecked(value.preferredChargerTypes.contains(static_cast<int>(ChargerType::Fast)));
    reminderStartEdit_->setTime(timeFromText(value.reminderStartTime, QTime(8, 0)));
    reminderEndEdit_->setTime(timeFromText(value.reminderEndTime, QTime(22, 0)));
    minIdleSpin_->setValue(value.minIdleChargers);
    dndStartEdit_->setTime(timeFromText(value.dndStartTime, QTime(22, 0)));
    dndEndEdit_->setTime(timeFromText(value.dndEndTime, QTime(7, 0)));
    refreshHomeLocationUi();
}

void UserPreferenceDialog::setHomeFromCurrentLocation()
{
    if (!hasCurrentLocation_) {
        showMessage(QStringLiteral("请先在首页完成定位"), true);
        refreshHomeLocationUi();
        return;
    }
    homeLocationDirty_ = true;
    homeSet_ = true;
    homeLatitude_ = currentLatitude_;
    homeLongitude_ = currentLongitude_;
    refreshHomeLocationUi();
}

void UserPreferenceDialog::clearHomeLocation()
{
    homeLocationDirty_ = true;
    homeSet_ = false;
    homeLatitude_ = 0.0;
    homeLongitude_ = 0.0;
    refreshHomeLocationUi();
}

void UserPreferenceDialog::refreshHomeLocationUi()
{
    const QString finalText = homeSet_
        ? QStringLiteral("已设置：%1, %2").arg(homeLatitude_, 0, 'f', 6)
              .arg(homeLongitude_, 0, 'f', 6)
        : QStringLiteral("未设置");
    homeStatusLabel_->setText(finalText);
    setHomeButton_->setEnabled(hasCurrentLocation_);
    clearHomeButton_->setEnabled(homeSet_);
}

void UserPreferenceDialog::save()
{
    if (saving_) return;
    UserPreference value = preference_;
    value.hasHomeLocation = homeSet_;
    value.homeLatitudeSet = homeSet_;
    value.homeLongitudeSet = homeSet_;
    value.homeLatitude = homeLatitude_;
    value.homeLongitude = homeLongitude_;
    value.homeRadiusKm = radiusSpin_->value();
    value.preferredChargerTypes.clear();
    if (slowCheck_->isChecked()) value.preferredChargerTypes.append(static_cast<int>(ChargerType::Slow));
    if (fastCheck_->isChecked()) value.preferredChargerTypes.append(static_cast<int>(ChargerType::Fast));
    value.reminderStartTime = reminderStartEdit_->time().toString(QStringLiteral("HH:mm"));
    value.reminderEndTime = reminderEndEdit_->time().toString(QStringLiteral("HH:mm"));
    value.minIdleChargers = minIdleSpin_->value();
    value.dndStartTime = dndStartEdit_->time().toString(QStringLiteral("HH:mm"));
    value.dndEndTime = dndEndEdit_->time().toString(QStringLiteral("HH:mm"));
    value.enabled = enabledCheck_->isChecked();
    saving_ = true;
    homeLocationDirty_ = true;
    saveButton_->setEnabled(false);
    showMessage(QStringLiteral("正在保存…"));
    facade_.updatePreference(value);
}

QString UserPreferenceDialog::stationName(qint64 stationId) const
{
    for (const Station &station : knownStations_) {
        if (station.id == stationId) return station.name;
    }
    return QStringLiteral("站点 #%1").arg(stationId);
}

void UserPreferenceDialog::showFavorites()
{
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("已关注站点"));
    dialog.setMinimumWidth(340);
    auto *layout = new QVBoxLayout(&dialog);
    auto *list = new QListWidget(&dialog);
    for (qint64 id : favoriteIds_) {
        auto *item = new QListWidgetItem(stationName(id), list);
        item->setData(Qt::UserRole, id);
    }
    layout->addWidget(list);
    auto *remove = new QPushButton(QStringLiteral("取消关注当前站点"), &dialog);
    remove->setObjectName(QStringLiteral("ghostButton"));
    remove->setEnabled(!favoriteIds_.isEmpty());
    layout->addWidget(remove);
    connect(remove, &QPushButton::clicked, &dialog, [this, &dialog, list, remove] {
        const int row = list->currentRow();
        if (row < 0 || row >= favoriteIds_.size()) return;
        remove->setEnabled(false);
        facade_.removeFavoriteStation(favoriteIds_.at(row));
        QTimer::singleShot(5000, &dialog, [remove] { remove->setEnabled(true); });
    });
    connect(&facade_, &UserClientFacade::favoriteStationChanged, &dialog,
            [this, list, remove](qint64 stationId, bool added) {
                if (added) return;
                for (int row = 0; row < list->count(); ++row) {
                    if (list->item(row)->data(Qt::UserRole).toLongLong() != stationId) continue;
                    delete list->takeItem(row);
                    break;
                }
                remove->setEnabled(!favoriteIds_.isEmpty());
                favoriteCountLabel_->setText(QStringLiteral("已关注 %1 个站点")
                                                 .arg(favoriteIds_.size()));
            });
    dialog.exec();
}

void UserPreferenceDialog::showMessage(const QString &message, bool error)
{
    stateLabel_->setObjectName(error ? QStringLiteral("statusError")
                                     : QStringLiteral("statusSuccess"));
    stateLabel_->style()->unpolish(stateLabel_);
    stateLabel_->style()->polish(stateLabel_);
    stateLabel_->setText(message);
}

}  // namespace ncs
