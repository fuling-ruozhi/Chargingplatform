#pragma once

#include "model/admin.h"

#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QTimer;

namespace ncs {

class AdminClientFacade;

class AdminLoginWindow : public QWidget
{
    Q_OBJECT
public:
    explicit AdminLoginWindow(AdminClientFacade &facade, QWidget *parent = nullptr);
    void setServerReady(bool ready);
    void setServerError(const QString &message);
    void resetForLogout();

signals:
    void loginCompleted(const ncs::Admin &admin);

private:
    void submit();
    void finish(const QString &message, bool error);
    void beginLock(int seconds);
    void updateLock();

    AdminClientFacade &facade_;
    QLineEdit *username_ = nullptr;
    QLineEdit *password_ = nullptr;
    QPushButton *loginButton_ = nullptr;
    QLabel *status_ = nullptr;
    QTimer *requestTimer_ = nullptr;
    QTimer *lockTimer_ = nullptr;
    int lockRemaining_ = 0;
    bool serverReady_ = false;
    bool requestPending_ = false;
};

}  // namespace ncs
