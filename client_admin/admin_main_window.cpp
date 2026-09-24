#include "admin_main_window.h"

#include "confirm_action_dialog.h"
#include "pages/charger_page.h"
#include "pages/charger_status_page.h"
#include "pages/predict_page.h"
#include "pages/log_page.h"
#include "pages/revenue_page.h"
#include "pages/station_page.h"
#include "pages/user_page.h"
#include "icon_assets.h"
#include "service/admin_client_facade.h"
#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

namespace ncs {
namespace {
struct PageSpec {
    const char *title;
    const char *subtitle;
    uiicons::Symbol icon;
    QWidget *(*factory)(QWidget *);
};
template<class Page>
QWidget *createPage(QWidget *parent)
{
    return new Page(parent);
}
const QList<PageSpec> pages = {
    {QT_TRANSLATE_NOOP("ncs::AdminMainWindow", "营收分析"),
     QT_TRANSLATE_NOOP("ncs::AdminMainWindow", "实时查看平台营收、订单与业务趋势"),
     uiicons::Symbol::Analytics, createPage<RevenuePage>},
    {QT_TRANSLATE_NOOP("ncs::AdminMainWindow", "电桩状态"),
     QT_TRANSLATE_NOOP("ncs::AdminMainWindow", "掌握充电设备运行状态与平台健康度"),
     uiicons::Symbol::Status, createPage<ChargerStatusPage>},
    {QT_TRANSLATE_NOOP("ncs::AdminMainWindow", "充电桩管理"),
     QT_TRANSLATE_NOOP("ncs::AdminMainWindow", "查看和维护平台充电设备"),
     uiicons::Symbol::Charger, createPage<ChargerPage>},
    {QT_TRANSLATE_NOOP("ncs::AdminMainWindow", "充电站管理"),
     QT_TRANSLATE_NOOP("ncs::AdminMainWindow", "管理充电站基础信息与设备分布"),
     uiicons::Symbol::Station, createPage<StationPage>},
    {QT_TRANSLATE_NOOP("ncs::AdminMainWindow", "用户管理"),
     QT_TRANSLATE_NOOP("ncs::AdminMainWindow", "查看用户状态与充电记录"),
     uiicons::Symbol::Users, createPage<UserPage>},
    {QT_TRANSLATE_NOOP("ncs::AdminMainWindow", "智能预测"),
     QT_TRANSLATE_NOOP("ncs::AdminMainWindow", "基于历史业务数据分析未来充电负载"),
     uiicons::Symbol::Prediction, createPage<PredictPage>},
    {QT_TRANSLATE_NOOP("ncs::AdminMainWindow", "日志审计"),
     QT_TRANSLATE_NOOP("ncs::AdminMainWindow", "查看登录、操作、安全与系统运行日志"),
     uiicons::Symbol::Status, createPage<LogPage>}
};
QPushButton *iconButton(const QString &name, const QIcon &icon, const QString &tip,
                        QWidget *parent, bool square = true)
{
    auto *button = new QPushButton(parent);
    button->setObjectName(name);
    button->setIcon(icon);
    button->setIconSize(QSize(22, 22));
    button->setToolTip(tip);
    if (square) {
        button->setFixedSize(44, 44);
    } else {
        button->setMinimumHeight(44);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
    return button;
}
} // namespace

AdminMainWindow::AdminMainWindow(const Admin &admin, AdminClientFacade &facade,
                                 QWidget *parent)
    : QMainWindow(parent), admin_(admin), facade_(facade)
{
    auto *root = setupWindow();
    root->addWidget(setupNavigation());
    root->addWidget(setupContent(), 1);
    setupStatusBar();
    selectPage(0);
    connect(&facade_, &AdminClientFacade::summaryReceived, this,
            [this](const AdminSummary &summary) {
                summaryPending_ = false;
                if (refreshButton_) refreshButton_->setEnabled(true);
                databaseStatus_->setText(tr("数据库：%1").arg(summary.databasePath));
                auto *chargerStatus = findChild<QLabel *>(
                    QStringLiteral("adminChargerLabel"));
                if (chargerStatus) {
                    chargerStatus->setText(tr("在线电桩：%1 / %2")
                        .arg(summary.onlineChargers).arg(summary.totalChargers));
                }
            });
    connect(&facade_, &AdminClientFacade::requestFailed, this,
            [this](const QString &route, int, const QString &message, int) {
                if (route == QStringLiteral("admin.summary")) {
                    summaryPending_ = false;
                    if (refreshButton_) refreshButton_->setEnabled(true);
                    databaseStatus_->setText(message);
                } else if (route == QStringLiteral("admin.logout")) {
                    if (confirmation_) confirmation_->finishFailure(message);
                    if (logoutButton_) logoutButton_->setEnabled(true);
                }
            });
    connect(&facade_, &AdminClientFacade::logoutSucceeded, this, [this] {
        if (confirmation_) confirmation_->finishSuccess();
    });
    connect(&facade_, &AdminClientFacade::networkError, this,
            [this](const QString &message) {
                summaryPending_ = false;
                if (refreshButton_) refreshButton_->setEnabled(true);
                if (logoutButton_) logoutButton_->setEnabled(true);
                if (confirmation_) confirmation_->finishFailure(message);
                databaseStatus_->setText(message);
            });
    QTimer::singleShot(0, this, &AdminMainWindow::requestSummary);
}

QHBoxLayout *AdminMainWindow::setupWindow()
{
    setObjectName("rootWindow");
    setWindowTitle(tr("NCS 管理端 · %1").arg(admin_.username));
    setMinimumSize(1280, 800);
    resize(1440, 900);
    statusBar()->setSizeGripEnabled(false);
    auto *central = new QWidget(this);
    central->setObjectName("rootWidget");
    setCentralWidget(central);
    auto *root = new QHBoxLayout(central);
    root->setContentsMargins(28, 24, 28, 10);
    root->setSpacing(22);
    return root;
}

QWidget *AdminMainWindow::setupNavigation()
{
    auto *rail = new QFrame(centralWidget());
    rail->setObjectName("navigationRail");
    rail->setMinimumWidth(108);
    rail->setMaximumWidth(108);
    auto *railLayout = new QVBoxLayout(rail);
    railLayout->setContentsMargins(8, 18, 8, 18);
    railLayout->setSpacing(8);
    auto *logo = new QLabel("N", rail);
    logo->setObjectName("brandLogo");
    logo->setAlignment(Qt::AlignCenter);
    logo->setFixedSize(44, 44);
    railLayout->addWidget(logo, 0, Qt::AlignHCenter);
    railLayout->addSpacing(20);
    for (int index = 0; index < pages.size(); ++index) {
        auto *button = iconButton(QString("adminNavButton%1").arg(index),
                                 uiicons::icon(pages[index].icon, true),
                                 tr(pages[index].title), rail, false);
        button->setIconSize(QSize(18, 18));
        button->setText(tr(pages[index].title));
        button->setCheckable(true);
        button->setProperty("navButton", true);
        navigationButtons_.append(button);
        railLayout->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, index] {
            selectPage(index);
        });
    }
    railLayout->addStretch();
    auto *help = iconButton("helpButton", uiicons::icon(uiicons::Symbol::Help), tr("帮助"), rail);
    help->setEnabled(false);
    railLayout->addWidget(help);
    logoutButton_ = iconButton("adminLogoutButton", uiicons::icon(uiicons::Symbol::Logout), tr("退出登录"), rail);
    logoutButton_->setProperty("logoutButton", true);
    railLayout->addWidget(logoutButton_);
    connect(logoutButton_, &QPushButton::clicked,
            this, &AdminMainWindow::confirmLogout);
    return rail;
}

QWidget *AdminMainWindow::setupContent()
{
    auto *content = new QWidget(centralWidget());
    content->setObjectName("mainContent");
    auto *main = new QVBoxLayout(content);
    main->setContentsMargins(0, 4, 0, 0);
    main->setSpacing(16);
    auto *header = new QWidget(content);
    header->setObjectName("pageHeader");
    auto *headerRow = new QHBoxLayout(header);
    headerRow->setContentsMargins(0, 0, 0, 0);
    headerRow->setSpacing(12);
    auto *titles = new QVBoxLayout;
    titles->setSpacing(4);
    pageTitle_ = new QLabel(header);
    pageTitle_->setObjectName("pageTitle");
    pageSubtitle_ = new QLabel(header);
    pageSubtitle_->setObjectName("pageSubtitle");
    titles->addWidget(pageTitle_);
    titles->addWidget(pageSubtitle_);
    headerRow->addLayout(titles);
    headerRow->addStretch();
    refreshButton_ = iconButton("adminRefreshButton", uiicons::icon(uiicons::Symbol::Refresh),
                               tr("刷新当前页面"), header);
    refreshButton_->setProperty("headerIcon", true);
    headerRow->addWidget(refreshButton_);
    headerRow->addSpacing(8);
    auto *avatar = new QLabel("A", header);
    avatar->setObjectName("adminAvatar");
    avatar->setAlignment(Qt::AlignCenter);
    avatar->setFixedSize(40, 40);
    headerRow->addWidget(avatar);
    auto *admin = new QLabel(admin_.username, header);
    admin->setObjectName("adminName");
    headerRow->addWidget(admin);
    main->addWidget(header);
    stacked_ = new QStackedWidget(content);
    stacked_->setObjectName("adminPages");
    for (const auto &page : pages) {
        QWidget *pageWidget = page.factory(stacked_);
        stacked_->addWidget(pageWidget);
        if (auto *revenue = qobject_cast<RevenuePage *>(pageWidget)) {
            revenue->bindFacade(facade_);
        } else if (auto *status = qobject_cast<ChargerStatusPage *>(pageWidget)) {
            status->bindFacade(facade_);
        } else if (auto *charger = qobject_cast<ChargerPage *>(pageWidget)) {
            charger->bindFacade(facade_);
        } else if (auto *station = qobject_cast<StationPage *>(pageWidget)) {
            station->bindFacade(facade_);
        } else if (auto *user = qobject_cast<UserPage *>(pageWidget)) {
            user->bindFacade(facade_);
        } else if (auto *predict = qobject_cast<PredictPage *>(pageWidget)) {
            predict->bindFacade(facade_);
        } else if (auto *log = qobject_cast<LogPage *>(pageWidget)) {
            log->bindFacade(facade_);
        }
    }
    main->addWidget(stacked_, 1);
    connect(refreshButton_, &QPushButton::clicked, this, [this] {
        statusBar()->showMessage(tr("已刷新"), 2000);
        requestSummary();
        if (auto *revenue = qobject_cast<RevenuePage *>(stacked_->currentWidget())) {
            revenue->refresh();
        } else if (auto *status = qobject_cast<ChargerStatusPage *>(stacked_->currentWidget())) {
            status->refresh();
        } else if (auto *charger = qobject_cast<ChargerPage *>(stacked_->currentWidget())) {
            charger->refresh();
        } else if (auto *station = qobject_cast<StationPage *>(stacked_->currentWidget())) {
            station->refresh();
        } else if (auto *user = qobject_cast<UserPage *>(stacked_->currentWidget())) {
            user->refresh();
        } else if (auto *predict = qobject_cast<PredictPage *>(stacked_->currentWidget())) {
            predict->refresh();
        } else if (auto *log = qobject_cast<LogPage *>(stacked_->currentWidget())) {
            log->refresh();
        }
        emit refreshRequested(stacked_->currentIndex());
    });
    return content;
}

void AdminMainWindow::setupStatusBar()
{
    auto *serverWrap = new QWidget(this);
    auto *serverRow = new QHBoxLayout(serverWrap);
    serverRow->setContentsMargins(0, 0, 0, 0);
    serverRow->setSpacing(7);
    serverDot_ = new QLabel(serverWrap);
    serverDot_->setObjectName("serverDot");
    serverDot_->setFixedSize(6, 6);
    serverStatus_ = new QLabel(tr("服务器离线"), serverWrap);
    serverRow->addWidget(serverDot_);
    serverRow->addWidget(serverStatus_);
    databaseStatus_ = new QLabel(tr("数据库已就绪"), this);
    databaseStatus_->setObjectName("adminDatabaseLabel");
    databaseStatus_->setToolTip(tr("数据库已就绪"));
    auto *chargerStatus = new QLabel(tr("在线电桩：读取中…"), this);
    chargerStatus->setObjectName("adminChargerLabel");
    timeLabel_ = new QLabel(this);
    timeLabel_->setObjectName("adminClockLabel");
    statusBar()->addWidget(serverWrap);
    statusBar()->addWidget(databaseStatus_);
    statusBar()->addPermanentWidget(chargerStatus);
    statusBar()->addPermanentWidget(timeLabel_);
    clockTimer_ = new QTimer(this);
    clockTimer_->setInterval(1000);
    connect(clockTimer_, &QTimer::timeout, this, [this] {
        timeLabel_->setText(QDateTime::currentDateTime().toString("HH:mm"));
    });
    clockTimer_->start();
    timeLabel_->setText(QDateTime::currentDateTime().toString("HH:mm"));
}

void AdminMainWindow::selectPage(int index)
{
    if (index < 0 || index >= stacked_->count()
        || index >= static_cast<int>(pages.size())) {
        return;
    }
    stacked_->setCurrentIndex(index);
    pageTitle_->setText(tr(pages[index].title));
    pageSubtitle_->setText(tr(pages[index].subtitle));
    for (int i = 0; i < navigationButtons_.size(); ++i) {
        navigationButtons_[i]->setChecked(i == index);
    }
}

void AdminMainWindow::setServerStatus(bool ready, quint16 port)
{
    serverDot_->setProperty("online", ready);
    serverDot_->style()->unpolish(serverDot_);
    serverDot_->style()->polish(serverDot_);
    serverStatus_->setText(ready ? tr("服务器在线") : tr("服务器离线"));
    serverStatus_->setToolTip(ready ? QString("127.0.0.1:%1").arg(port) : QString());
}

void AdminMainWindow::setDatabaseStatus(const QString &status)
{
    databaseStatus_->setText(status.isEmpty() ? tr("数据库已就绪") : status);
}
void AdminMainWindow::requestSummary()
{
    if (summaryPending_) return;
    summaryPending_ = true;
    if (refreshButton_) refreshButton_->setEnabled(false);
    databaseStatus_->setText(tr("正在刷新运行状态…"));
    facade_.requestSummary();
}

void AdminMainWindow::confirmLogout()
{
    if (confirmation_) return;
    auto *dialog = new ConfirmActionDialog(
        tr("退出管理后台"),
        tr("确认结束当前管理员会话吗？"),
        tr("确认退出"), ConfirmActionDialog::Severity::Danger,
        this);
    confirmation_ = dialog;
    connect(dialog, &QDialog::finished, this, [this, dialog] {
        confirmation_.clear();
        QTimer::singleShot(0, this, [dialog] { delete dialog; });
    });
    connect(dialog, &ConfirmActionDialog::confirmed, this, [this] {
        if (logoutButton_) logoutButton_->setEnabled(false);
        statusBar()->showMessage(tr("正在安全退出…"));
        emit logoutRequested();
    });
    dialog->open();
}

} // namespace ncs
