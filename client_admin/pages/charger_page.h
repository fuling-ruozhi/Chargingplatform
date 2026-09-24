#pragma once

#include "../../core/model/charger.h"

#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QShowEvent;
class QTableWidget;

namespace ncs {

class AdminClientFacade;
struct FormalAdminUiTestAccess;

class ChargerPage : public QWidget
{
    Q_OBJECT
public:
    explicit ChargerPage(QWidget *parent = nullptr);
    void bindFacade(AdminClientFacade &facade);
    void refresh();

protected:
    void showEvent(QShowEvent *event) override;

private:
    friend struct FormalAdminUiTestAccess;

    void requestList();
    void applyChargers(const QVector<Charger> &chargers);
    void updateActions();
    qint64 selectedId() const;
    void showCreateDialog();

    AdminClientFacade *facade_ = nullptr;
    QLineEdit *search_ = nullptr;
    QComboBox *statusFilter_ = nullptr;
    QTableWidget *table_ = nullptr;
    QLabel *stateLabel_ = nullptr;
    QPushButton *filterButton_ = nullptr;
    QPushButton *refreshButton_ = nullptr;
    QPushButton *deleteButton_ = nullptr;
    QPushButton *faultButton_ = nullptr;
    QPushButton *recoverButton_ = nullptr;
    QPushButton *restartButton_ = nullptr;
    bool loaded_ = false;
};

}  // namespace ncs
