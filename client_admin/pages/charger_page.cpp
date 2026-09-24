#include "charger_page.h"

#include "../icon_assets.h"
#include "../../core/service/admin_client_facade.h"
#include "dashboard_ui.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QAbstractItemView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace ncs {
namespace {

QTableWidgetItem *textItem(const QString &text)
{
    return new QTableWidgetItem(text);
}

}

ChargerPage::ChargerPage(QWidget *parent) : QWidget(parent)
{
    auto *content = new QWidget;
    content->setObjectName("pageCanvas");
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 4, 8);
    layout->setSpacing(16);

    auto *filterBar = new QFrame(content);
    filterBar->setObjectName(QStringLiteral("filterBar"));
    auto *tools = new QHBoxLayout(filterBar);
    tools->setContentsMargins(14, 10, 14, 10);
    tools->setSpacing(10);
    search_ = new QLineEdit(content);
    search_->setObjectName("searchField");
    search_->setPlaceholderText(tr("搜索电桩编号或所属站点"));
    search_->setMaxLength(128);
    search_->addAction(uiicons::icon(uiicons::Symbol::Search),
                       QLineEdit::LeadingPosition);
    search_->setMaximumWidth(360);
    tools->addWidget(search_);

    statusFilter_ = new QComboBox(content);
    statusFilter_->setObjectName("chargerStatusFilter");
    statusFilter_->addItem(tr("全部状态"), -1);
    statusFilter_->addItem(tr("空闲"), static_cast<int>(ChargerStatus::Idle));
    statusFilter_->addItem(tr("使用中"), static_cast<int>(ChargerStatus::Using));
    statusFilter_->addItem(tr("故障"), static_cast<int>(ChargerStatus::Fault));
    statusFilter_->setMinimumWidth(120);
    tools->addWidget(statusFilter_);
    tools->addStretch();

    filterButton_ = new QPushButton(tr("筛选"), content);
    filterButton_->setObjectName("chargerFilterButton");
    filterButton_->setProperty("buttonRole", "secondary");
    tools->addWidget(filterButton_);

    refreshButton_ = new QPushButton(tr("刷新"), content);
    refreshButton_->setObjectName("chargerRefreshButton");
    refreshButton_->setProperty("buttonRole", "secondary");
    tools->addWidget(refreshButton_);
    auto *addButton = new QPushButton(tr("新增电桩"), content);
    addButton->setObjectName("chargerAddButton");
    addButton->setProperty("buttonRole", "primary");
    tools->addWidget(addButton);
    layout->addWidget(filterBar);

    auto *helper = new QLabel(
        tr("支持按电桩编号或所属站点查询，状态筛选会实时更新列表"), content);
    helper->setObjectName("secondaryText");
    layout->addWidget(helper);

    auto *actionBar = new QFrame(content);
    actionBar->setObjectName(QStringLiteral("actionBar"));
    auto *actions = new QHBoxLayout(actionBar);
    actions->setContentsMargins(14, 8, 14, 8);
    actions->setSpacing(8);
    const auto addAction = [&](const QString &name, const QString &title) {
        auto *button = new QPushButton(title, content);
        button->setObjectName(name);
        button->setProperty("buttonRole", "secondary");
        button->setEnabled(false);
        actions->addWidget(button);
        return button;
    };
    deleteButton_ = addAction("chargerDeleteButton", tr("删除"));
    deleteButton_->setProperty("buttonRole", "danger");
    faultButton_ = addAction("chargerFaultButton", tr("标记故障"));
    faultButton_->setProperty("buttonRole", "danger");
    recoverButton_ = addAction("chargerRecoverButton", tr("恢复"));
    recoverButton_->setProperty("buttonRole", "success");
    restartButton_ = addAction("chargerRestartButton", tr("远程重启"));
    actions->addStretch();
    layout->addWidget(actionBar);

    stateLabel_ = new QLabel(tr("尚未加载电桩数据"), content);
    stateLabel_->setObjectName("chargerStateLabel");
    stateLabel_->setProperty("state", "empty");
    layout->addWidget(stateLabel_);

    table_ = new QTableWidget(content);
    table_->setObjectName("chargerTable");
    table_->setColumnCount(8);
    table_->setHorizontalHeaderLabels({tr("电桩编号"), tr("所属站点"), tr("类型"),
                                       tr("功率"), tr("状态"), tr("累计次数"),
                                       tr("累计时长"), tr("操作")});
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setShowGrid(false);
    table_->verticalHeader()->setVisible(false);
    table_->verticalHeader()->setDefaultSectionSize(48);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->setMinimumHeight(490);
    layout->addWidget(table_, 1);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(dashboard::scrollPage(content, this));

    connect(search_, &QLineEdit::textChanged, this, [this] { requestList(); });
    connect(statusFilter_, &QComboBox::currentIndexChanged, this,
            [this] { requestList(); });
    connect(filterButton_, &QPushButton::clicked, this, [this] {
        stateLabel_->setText(tr("正在应用筛选条件…"));
        requestList();
    });
    connect(refreshButton_, &QPushButton::clicked, this, &ChargerPage::refresh);
    connect(addButton, &QPushButton::clicked, this, &ChargerPage::showCreateDialog);
    connect(table_, &QTableWidget::itemSelectionChanged, this,
            &ChargerPage::updateActions);
    connect(deleteButton_, &QPushButton::clicked, this, [this] {
        const qint64 id = selectedId();
        if (id <= 0 || !facade_) return;
        if (QMessageBox::question(this, tr("确认删除"),
                                  tr("确定删除选中的电桩吗？")) == QMessageBox::Yes) {
            stateLabel_->setText(tr("正在删除…"));
            facade_->deleteCharger(id);
        }
    });
    connect(faultButton_, &QPushButton::clicked, this, [this] {
        if (facade_) facade_->markChargerFault(selectedId());
    });
    connect(recoverButton_, &QPushButton::clicked, this, [this] {
        if (facade_) facade_->recoverCharger(selectedId());
    });
    connect(restartButton_, &QPushButton::clicked, this, [this] {
        if (facade_) facade_->restartCharger(selectedId());
    });
}

void ChargerPage::bindFacade(AdminClientFacade &facade)
{
    facade_ = &facade;
    connect(facade_, &AdminClientFacade::chargersReceived,
            this, &ChargerPage::applyChargers);
    connect(facade_, &AdminClientFacade::chargerActionSucceeded, this,
            [this](const QString &, qint64) {
        stateLabel_->setText(tr("操作成功，列表已刷新"));
        requestList();
    });
    connect(facade_, &AdminClientFacade::requestFailed, this,
            [this](const QString &route, int, const QString &message, int) {
        if (!route.startsWith(QStringLiteral("admin.charger."))) return;
        refreshButton_->setEnabled(true);
        filterButton_->setEnabled(true);
        stateLabel_->setText(message);
        stateLabel_->setProperty("state", "error");
        stateLabel_->style()->unpolish(stateLabel_);
        stateLabel_->style()->polish(stateLabel_);
        updateActions();
    });
}

void ChargerPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!loaded_) {
        loaded_ = true;
        requestList();
    }
}

void ChargerPage::requestList()
{
    if (!facade_) return;
    refreshButton_->setEnabled(false);
    filterButton_->setEnabled(false);
    stateLabel_->setText(tr("正在加载电桩列表…"));
    stateLabel_->setProperty("state", "loading");
    stateLabel_->style()->unpolish(stateLabel_);
    stateLabel_->style()->polish(stateLabel_);
    facade_->requestChargers(search_->text(), statusFilter_->currentData().toInt());
    updateActions();
}

void ChargerPage::refresh()
{
    loaded_ = true;
    requestList();
}

void ChargerPage::applyChargers(const QVector<Charger> &chargers)
{
    refreshButton_->setEnabled(true);
    filterButton_->setEnabled(true);
    table_->setRowCount(0);
    for (const Charger &charger : chargers) {
        const int row = table_->rowCount();
        table_->insertRow(row);
        auto *code = textItem(charger.code);
        code->setData(Qt::UserRole, charger.id);
        table_->setItem(row, 0, code);
        table_->setItem(row, 1, textItem(charger.stationName));
        table_->setItem(row, 2, textItem(chargerTypeText(charger.type)));
        table_->setItem(row, 3, textItem(tr("%1 kW").arg(charger.powerKw, 0, 'f', 1)));
        auto *statusItem = textItem(chargerStatusText(charger.status));
        statusItem->setData(Qt::UserRole, static_cast<int>(charger.status));
        table_->setItem(row, 4, statusItem);
        table_->setItem(row, 5, textItem(QString::number(charger.totalCount)));
        table_->setItem(row, 6, textItem(tr("%1 分钟").arg(charger.totalMinutes)));
        auto *action = new QLabel(charger.status == ChargerStatus::Using
                                       ? tr("使用中") : tr("可操作"), table_);
        action->setObjectName("secondaryText");
        table_->setCellWidget(row, 7, action);
    }
    stateLabel_->setProperty("state", chargers.isEmpty() ? "empty" : "normal");
    stateLabel_->setText(chargers.isEmpty() ? tr("未找到符合条件的电桩")
                                           : tr("共 %1 个电桩").arg(chargers.size()));
    stateLabel_->style()->unpolish(stateLabel_);
    stateLabel_->style()->polish(stateLabel_);
    updateActions();
}

qint64 ChargerPage::selectedId() const
{
    const int row = table_ ? table_->currentRow() : -1;
    if (row < 0 || table_->selectedItems().isEmpty() || !table_->item(row, 0)) return 0;
    return table_->item(row, 0)->data(Qt::UserRole).toLongLong();
}

void ChargerPage::updateActions()
{
    const int row = table_ ? table_->currentRow() : -1;
    if (selectedId() <= 0 || row < 0 || !table_->item(row, 0)) {
        deleteButton_->setEnabled(false);
        faultButton_->setEnabled(false);
        recoverButton_->setEnabled(false);
        restartButton_->setEnabled(false);
        return;
    }
    const auto status = static_cast<ChargerStatus>(
        table_->item(row, 4)->data(Qt::UserRole).toInt());
    const bool usingCharger = status == ChargerStatus::Using;
    const bool fault = status == ChargerStatus::Fault;
    deleteButton_->setEnabled(!usingCharger);
    faultButton_->setEnabled(!usingCharger && !fault);
    recoverButton_->setEnabled(fault);
    restartButton_->setEnabled(!usingCharger);
}

void ChargerPage::showCreateDialog()
{
    if (!facade_) return;
    QDialog dialog(this);
    dialog.setWindowTitle(tr("新增电桩"));
    auto *root = dashboard::modalLayout(&dialog, tr("新增电桩"),
                                        tr("填写电桩基础信息并关联所属站点"));
    auto *form = new QVBoxLayout;
    form->setSpacing(12);
    auto *station = new QLineEdit(&dialog);
    station->setObjectName(QStringLiteral("formInput"));
    station->setPlaceholderText(tr("请输入站点 ID"));
    station->setMaxLength(18);
    auto *code = new QLineEdit(&dialog);
    code->setObjectName(QStringLiteral("formInput"));
    code->setMaxLength(64);
    code->setPlaceholderText(tr("请输入电桩编号"));
    auto *type = new QComboBox(&dialog);
    type->setObjectName(QStringLiteral("formInput"));
    type->addItem(tr("交流慢充"), 0);
    type->addItem(tr("直流快充"), 1);
    auto *power = new QDoubleSpinBox(&dialog);
    power->setObjectName(QStringLiteral("formInput"));
    power->setRange(0.1, 1000.0);
    power->setValue(7.0);
    power->setSuffix(tr(" kW"));
    dashboard::addModalField(form, &dialog, tr("站点 ID"), station);
    dashboard::addModalField(form, &dialog, tr("电桩编号"), code);
    dashboard::addModalField(form, &dialog, tr("类型"), type);
    dashboard::addModalField(form, &dialog, tr("功率"), power);
    root->addLayout(form);

    auto *error = new QLabel(&dialog);
    error->setObjectName(QStringLiteral("formError"));
    error->setWordWrap(true);
    error->hide();
    root->addWidget(error);

    auto *buttons = dashboard::modalButtons(&dialog, tr("新增电桩"), tr("取消"));
    root->addWidget(buttons, 0, Qt::AlignRight);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        bool ok = false;
        const qint64 stationId = station->text().toLongLong(&ok);
        if (!ok || stationId <= 0) {
            error->setText(tr("请输入有效的站点 ID"));
            error->show();
            station->setFocus();
            return;
        }
        if (code->text().trimmed().isEmpty()) {
            error->setText(tr("电桩编号不能为空"));
            error->show();
            code->setFocus();
            return;
        }
        if (power->value() <= 0.0) {
            error->setText(tr("功率必须大于 0"));
            error->show();
            power->setFocus();
            return;
        }
        error->hide();
        facade_->createCharger(stationId, code->text(), type->currentData().toInt(),
                               power->value());
        dialog.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    dialog.exec();
}

}  // namespace ncs
