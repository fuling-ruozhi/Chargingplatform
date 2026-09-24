#pragma once

#include "model/station.h"
#include "model/user_preference.h"

#include <QDialog>
#include <QHash>
#include <QVector>

class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QTimeEdit;

namespace ncs {

class UserClientFacade;

class UserPreferenceDialog : public QDialog
{
    Q_OBJECT

public:
    UserPreferenceDialog(UserClientFacade &facade, bool hasCurrentLocation,
                         double currentLongitude, double currentLatitude,
                         const QVector<Station> &knownStations = {},
                         QWidget *parent = nullptr);

signals:
    void preferenceSaved(const ncs::UserPreference &preference);

private:
    void loadPreference(const UserPreference &preference);
    void setHomeFromCurrentLocation();
    void clearHomeLocation();
    void refreshHomeLocationUi();
    void save();
    void showFavorites();
    QString stationName(qint64 stationId) const;
    void showMessage(const QString &message, bool error = false);

    UserClientFacade &facade_;
    bool hasCurrentLocation_ = false;
    double currentLongitude_ = 0.0;
    double currentLatitude_ = 0.0;
    QVector<Station> knownStations_;
    UserPreference preference_;
    QVector<qint64> favoriteIds_;
    bool saving_ = false;
    bool homeLocationDirty_ = false;
    bool homeSet_ = false;
    double homeLatitude_ = 0.0;
    double homeLongitude_ = 0.0;

    QCheckBox *enabledCheck_ = nullptr;
    QLabel *homeStatusLabel_ = nullptr;
    QLabel *favoriteCountLabel_ = nullptr;
    QLabel *stateLabel_ = nullptr;
    QPushButton *setHomeButton_ = nullptr;
    QPushButton *clearHomeButton_ = nullptr;
    QDoubleSpinBox *radiusSpin_ = nullptr;
    QCheckBox *slowCheck_ = nullptr;
    QCheckBox *fastCheck_ = nullptr;
    QTimeEdit *reminderStartEdit_ = nullptr;
    QTimeEdit *reminderEndEdit_ = nullptr;
    QSpinBox *minIdleSpin_ = nullptr;
    QTimeEdit *dndStartEdit_ = nullptr;
    QTimeEdit *dndEndEdit_ = nullptr;
    QPushButton *saveButton_ = nullptr;
};

}  // namespace ncs
