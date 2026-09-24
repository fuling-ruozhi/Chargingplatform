
#pragma once

#include "../../core/model/station.h"
#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QShowEvent;
class QTableWidget;

namespace ncs {
class AdminClientFacade;
class StationPage : public QWidget
{
    Q_OBJECT
public:
    explicit StationPage(QWidget *parent = nullptr);
    void bindFacade(AdminClientFacade &facade);
    void refresh();
protected:
    void showEvent(QShowEvent *event) override;
private:
    void requestList();
    void applyStations(const QVector<Station> &stations);
    void updateActions();
    qint64 selectedId() const;
    Station selectedStation() const;
    void showStationDialog(bool edit);
    void showChargers();
    void showBatchDialog();
    AdminClientFacade *facade_ = nullptr;
    QLineEdit *search_ = nullptr;
    QTableWidget *table_ = nullptr;
    QLabel *stateLabel_ = nullptr;
    QLabel *ratingLabel_ = nullptr;
    QPushButton *editButton_ = nullptr;
    QPushButton *deleteButton_ = nullptr;
    QPushButton *viewButton_ = nullptr;
    QPushButton *batchButton_ = nullptr;
    bool loaded_ = false;
};
}
