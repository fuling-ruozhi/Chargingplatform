#pragma once

#include "model/admin.h"

#include <QList>
#include <QMainWindow>
#include <QPointer>

class QHBoxLayout;
class QLabel;
class QPushButton;
class QStackedWidget;
class QTimer;

namespace ncs {

class AdminClientFacade;
class ConfirmActionDialog;
struct FormalAdminUiTestAccess;

class AdminMainWindow : public QMainWindow
{
    Q_OBJECT
public:
    AdminMainWindow(const Admin &admin, AdminClientFacade &facade,
                    QWidget *parent = nullptr);
    void setServerStatus(bool ready, quint16 port = 9527);
    void setDatabaseStatus(const QString &status);

signals:
    void refreshRequested(int pageIndex);
    void logoutRequested();

private:
    friend struct FormalAdminUiTestAccess;

    QHBoxLayout *setupWindow();
    QWidget *setupNavigation();
    QWidget *setupContent();
    void setupStatusBar();
    void selectPage(int index);
    void requestSummary();
    void confirmLogout();

    Admin admin_;
    AdminClientFacade &facade_;
    QLabel *pageTitle_ = nullptr;
    QLabel *pageSubtitle_ = nullptr;
    QLabel *databaseStatus_ = nullptr;
    QLabel *serverStatus_ = nullptr;
    QLabel *serverDot_ = nullptr;
    QLabel *timeLabel_ = nullptr;
    QStackedWidget *stacked_ = nullptr;
    QTimer *clockTimer_ = nullptr;
    QList<QPushButton *> navigationButtons_;
    QPushButton *refreshButton_ = nullptr;
    QPushButton *logoutButton_ = nullptr;
    QPointer<ConfirmActionDialog> confirmation_;
    bool summaryPending_ = false;
};

}  // namespace ncs
