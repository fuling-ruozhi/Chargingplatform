#include "user_page.h"
#include "../icon_assets.h"
#include "../../core/service/admin_client_facade.h"
#include "dashboard_ui.h"

#include <QAbstractItemView>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QStackedLayout>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace ncs {
namespace { QTableWidgetItem *cell(const QString &text) { return new QTableWidgetItem(text); } }

UserPage::UserPage(QWidget *parent) : QWidget(parent)
{
    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 4, 8);
    layout->setSpacing(12);
    auto *filterBar = new QFrame(content);
    filterBar->setObjectName(QStringLiteral("filterBar"));
    auto *tools = new QHBoxLayout(filterBar);
    tools->setContentsMargins(14, 10, 14, 10);
    search_ = new QLineEdit(content);
    search_->setObjectName(QStringLiteral("searchField"));
    search_->setPlaceholderText(tr("搜索用户 ID、用户名或手机号"));
    search_->setMaxLength(128);
    search_->addAction(uiicons::icon(uiicons::Symbol::Search), QLineEdit::LeadingPosition);
    search_->setMaximumWidth(380);
    tools->addWidget(search_);
    tools->addStretch();
    refreshButton_ = new QPushButton(tr("刷新"), content);
    refreshButton_->setObjectName(QStringLiteral("userRefreshButton"));
    refreshButton_->setProperty("buttonRole", "secondary");
    tools->addWidget(refreshButton_);
    layout->addWidget(filterBar);
    auto *actionBar = new QFrame(content);
    actionBar->setObjectName(QStringLiteral("actionBar"));
    auto *actions = new QHBoxLayout(actionBar);
    actions->setContentsMargins(14, 8, 14, 8);
    freezeButton_ = new QPushButton(tr("冻结用户"), content);
    freezeButton_->setObjectName(QStringLiteral("userFreezeButton"));
    freezeButton_->setProperty("buttonRole", "danger");
    unfreezeButton_ = new QPushButton(tr("解冻用户"), content);
    unfreezeButton_->setObjectName(QStringLiteral("userUnfreezeButton"));
    unfreezeButton_->setProperty("buttonRole", "success");
    ordersButton_ = new QPushButton(tr("查看订单"), content);
    ordersButton_->setObjectName(QStringLiteral("userOrdersButton"));
    for (auto *button : {freezeButton_, unfreezeButton_, ordersButton_}) {
        button->setEnabled(false);
        actions->addWidget(button);
    }
    actions->addStretch();
    layout->addWidget(actionBar);
    stateLabel_ = new QLabel(tr("尚未加载用户数据"), content);
    stateLabel_->setObjectName(QStringLiteral("userStateLabel"));
    layout->addWidget(stateLabel_);
    table_ = new QTableWidget(content);
    table_->setObjectName(QStringLiteral("userTable"));
    table_->setColumnCount(6);
    table_->setHorizontalHeaderLabels({tr("用户 ID"), tr("用户名"), tr("手机号"),
                                       tr("昵称"), tr("状态"), tr("注册时间")});
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setShowGrid(false);
    table_->verticalHeader()->setVisible(false);
    table_->verticalHeader()->setDefaultSectionSize(48);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    emptyState_ = new QWidget(content);
    emptyState_->setObjectName(QStringLiteral("userEmptyState"));
    auto *emptyLayout = new QVBoxLayout(emptyState_);
    emptyLayout->setContentsMargins(24, 40, 24, 40);
    emptyLayout->setSpacing(8);
    emptyLayout->addStretch();
    auto *emptyIcon = new QLabel(emptyState_);
    emptyIcon->setObjectName(QStringLiteral("emptyIcon"));
    emptyIcon->setPixmap(uiicons::icon(uiicons::Symbol::Users).pixmap(42, 42));
    emptyIcon->setAlignment(Qt::AlignCenter);
    auto *emptyTitle = new QLabel(tr("暂无用户数据"), emptyState_);
    emptyTitle->setObjectName(QStringLiteral("emptyTitle"));
    emptyTitle->setAlignment(Qt::AlignCenter);
    auto *emptyNote = new QLabel(tr("调整搜索条件或点击刷新重试"), emptyState_);
    emptyNote->setObjectName(QStringLiteral("emptyNote"));
    emptyNote->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyIcon);
    emptyLayout->addWidget(emptyTitle);
    emptyLayout->addWidget(emptyNote);
    emptyLayout->addStretch();
    contentStack_ = new QStackedLayout;
    contentStack_->addWidget(table_);
    contentStack_->addWidget(emptyState_);
    auto *contentFrame = new QFrame(content);
    contentFrame->setObjectName(QStringLiteral("tableContent"));
    contentFrame->setLayout(contentStack_);
    layout->addWidget(contentFrame, 1);
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(dashboard::scrollPage(content, this));

    connect(search_, &QLineEdit::textChanged, this, &UserPage::requestList);
    connect(refreshButton_, &QPushButton::clicked, this, &UserPage::refresh);
    connect(table_, &QTableWidget::itemSelectionChanged, this, &UserPage::updateActions);
    connect(freezeButton_, &QPushButton::clicked, this, [this] {
        if (QMessageBox::question(this, tr("确认冻结"), tr("冻结后该用户下次登录将被拒绝，继续吗？")) == QMessageBox::Yes) {
            stateLabel_->setText(tr("正在冻结用户…"));
            facade_->freezeUser(selectedId());
        }
    });
    connect(unfreezeButton_, &QPushButton::clicked, this, [this] {
        stateLabel_->setText(tr("正在解冻用户…"));
        facade_->unfreezeUser(selectedId());
    });
    connect(ordersButton_, &QPushButton::clicked, this, [this] {
        stateLabel_->setText(tr("正在加载用户订单…"));
        showOrders();
    });
}

void UserPage::bindFacade(AdminClientFacade &facade)
{
    facade_ = &facade;
    connect(&facade, &AdminClientFacade::usersReceived, this, &UserPage::applyUsers);
    connect(&facade, &AdminClientFacade::userActionSucceeded, this,
            [this](const QString &, qint64) { requestList(); });
    connect(&facade, &AdminClientFacade::userOrdersReceived, this,
            [this](qint64, const QVector<ChargingRecord> &orders) {
                QDialog dialog(this);
                dialog.setWindowTitle(tr("用户订单"));
                auto *layout = dashboard::modalLayout(&dialog, tr("用户订单"),
                                                     tr("查看当前用户的充电记录"));
                auto *label = new QLabel(orders.isEmpty() ? tr("该用户暂无订单")
                                                           : tr("共 %1 条订单").arg(orders.size()), &dialog);
                label->setObjectName(QStringLiteral("modalInfo"));
                layout->addWidget(label);
                auto *table = new QTableWidget(orders.size(), 6, &dialog);
                table->setObjectName(QStringLiteral("modalTable"));
                table->setHorizontalHeaderLabels({tr("订单号"), tr("站点"), tr("电桩"),
                                                   tr("开始"), tr("结束"), tr("金额")});
                table->setSelectionMode(QAbstractItemView::NoSelection);
                table->setEditTriggers(QAbstractItemView::NoEditTriggers);
                table->setShowGrid(false);
                table->verticalHeader()->setVisible(false);
                table->verticalHeader()->setDefaultSectionSize(44);
                table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
                for (int row = 0; row < orders.size(); ++row) {
                    const auto &order = orders.at(row);
                    table->setItem(row, 0, cell(order.orderNo));
                    table->setItem(row, 1, cell(order.stationName));
                    table->setItem(row, 2, cell(order.chargerCode));
                    table->setItem(row, 3, cell(order.startTime));
                    table->setItem(row, 4, cell(order.endTime));
                    table->setItem(row, 5, cell(QString::number(order.cost, 'f', 2)));
                }
                layout->addWidget(table);
                auto *buttons = dashboard::modalButtons(&dialog, tr("关闭"));
                layout->addWidget(buttons, 0, Qt::AlignRight);
                connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
                dialog.resize(820, 420);
                dialog.exec();
            });
    connect(&facade, &AdminClientFacade::requestFailed, this,
            [this](const QString &route, int, const QString &message, int) {
                if (route.startsWith(QStringLiteral("admin.user."))) {
                    refreshButton_->setEnabled(true);
                    stateLabel_->setText(message);
                    stateLabel_->setProperty("state", "error");
                    updateActions();
                }
            });
}

void UserPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!loaded_) { loaded_ = true; requestList(); }
}

void UserPage::requestList()
{
    if (!facade_) return;
    refreshButton_->setEnabled(false);
    stateLabel_->setText(tr("正在加载用户列表…"));
    stateLabel_->setProperty("state", "loading");
    facade_->requestUsers(search_->text());
    updateActions();
}

void UserPage::refresh() { loaded_ = true; requestList(); }

void UserPage::applyUsers(const QVector<User> &users)
{
    refreshButton_->setEnabled(true);
    table_->setRowCount(0);
    for (const User &user : users) {
        const int row = table_->rowCount();
        table_->insertRow(row);
        auto *id = cell(QString::number(user.id));
        id->setData(Qt::UserRole, user.id);
        id->setData(Qt::UserRole + 1, user.status);
        table_->setItem(row, 0, id);
        table_->setItem(row, 1, cell(user.username));
        table_->setItem(row, 2, cell(user.phone));
        table_->setItem(row, 3, cell(user.nickname));
        table_->setItem(row, 4, cell(user.status == 1 ? tr("正常") : tr("已冻结")));
        table_->setItem(row, 5, cell(user.createdAt));
    }
    stateLabel_->setProperty("state", users.isEmpty() ? "empty" : "normal");
    if (contentStack_) {
        contentStack_->setCurrentWidget(users.isEmpty() ? emptyState_ : table_);
    }
    stateLabel_->setText(users.isEmpty() ? tr("暂无用户数据") : tr("共 %1 个用户").arg(users.size()));
    stateLabel_->style()->unpolish(stateLabel_);
    stateLabel_->style()->polish(stateLabel_);
    updateActions();
}

qint64 UserPage::selectedId() const
{
    const int row = table_->currentRow();
    return row >= 0 && !table_->selectedItems().isEmpty() && table_->item(row, 0)
        ? table_->item(row, 0)->data(Qt::UserRole).toLongLong() : 0;
}

int UserPage::selectedStatus() const
{
    const int row = table_->currentRow();
    return row >= 0 && !table_->selectedItems().isEmpty() && table_->item(row, 0)
        ? table_->item(row, 0)->data(Qt::UserRole + 1).toInt() : -1;
}

void UserPage::updateActions()
{
    const bool selected = selectedId() > 0;
    freezeButton_->setEnabled(selected && selectedStatus() == 1);
    unfreezeButton_->setEnabled(selected && selectedStatus() == 0);
    ordersButton_->setEnabled(selected);
}

void UserPage::showOrders()
{
    if (facade_ && selectedId() > 0) facade_->requestUserOrders(selectedId());
}

}
