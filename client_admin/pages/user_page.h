#pragma once

#include "../../core/model/user.h"
#include "../../core/model/charging_record.h"
#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QShowEvent;
class QStackedLayout;
class QTableWidget;

namespace ncs {
class AdminClientFacade;
struct FormalAdminUiTestAccess;
class UserPage : public QWidget
{
    Q_OBJECT
public:
    explicit UserPage(QWidget *parent = nullptr);
    void bindFacade(AdminClientFacade &facade);
    void refresh();
protected:
    void showEvent(QShowEvent *event) override;
private:
    friend struct FormalAdminUiTestAccess;

    void requestList();
    void applyUsers(const QVector<User> &users);
    void updateActions();
    qint64 selectedId() const;
    int selectedStatus() const;
    void showOrders();
    AdminClientFacade *facade_ = nullptr;
    QLineEdit *search_ = nullptr;
    QTableWidget *table_ = nullptr;
    QStackedLayout *contentStack_ = nullptr;
    QWidget *emptyState_ = nullptr;
    QLabel *stateLabel_ = nullptr;
    QPushButton *refreshButton_ = nullptr;
    QPushButton *freezeButton_ = nullptr;
    QPushButton *unfreezeButton_ = nullptr;
    QPushButton *ordersButton_ = nullptr;
    bool loaded_ = false;
};
}
