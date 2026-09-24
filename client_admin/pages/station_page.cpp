#include "station_page.h"

#include "../icon_assets.h"
#include "../../core/service/admin_client_facade.h"
#include "../../core/service/station_rating_insight.h"
#include "dashboard_ui.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace ncs {
namespace {
QTableWidgetItem *cell(const QString &text)
{
    return new QTableWidgetItem(text);
}
}

StationPage::StationPage(QWidget *parent) : QWidget(parent)
{
    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 4, 8);
    layout->setSpacing(16);

    auto *filterBar = new QFrame(content);
    filterBar->setObjectName(QStringLiteral("filterBar"));
    auto *tools = new QHBoxLayout(filterBar);
    tools->setContentsMargins(14, 10, 14, 10);
    tools->setSpacing(10);
    search_ = new QLineEdit(content);
    search_->setObjectName(QStringLiteral("stationSearchField"));
    search_->setPlaceholderText(tr("搜索站点名称或地址"));
    search_->setMaxLength(128);
    search_->addAction(uiicons::icon(uiicons::Symbol::Search), QLineEdit::LeadingPosition);
    search_->setMaximumWidth(360);
    tools->addWidget(search_);
    tools->addStretch();
    auto *refresh = new QPushButton(tr("刷新"), content);
    refresh->setObjectName(QStringLiteral("stationRefreshButton"));
    refresh->setProperty("buttonRole", "secondary");
    tools->addWidget(refresh);
    auto *add = new QPushButton(tr("新增站点"), content);
    add->setObjectName(QStringLiteral("stationAddButton"));
    add->setProperty("buttonRole", "primary");
    tools->addWidget(add);
    layout->addWidget(filterBar);

    auto *actionBar = new QFrame(content);
    actionBar->setObjectName(QStringLiteral("actionBar"));
    auto *actions = new QHBoxLayout(actionBar);
    actions->setContentsMargins(14, 8, 14, 8);
    actions->setSpacing(8);
    const auto makeAction = [&](const QString &name, const QString &title) {
        auto *button = new QPushButton(title, content);
        button->setObjectName(name);
        button->setProperty("buttonRole", "secondary");
        button->setEnabled(false);
        actions->addWidget(button);
        return button;
    };
    editButton_ = makeAction(QStringLiteral("stationEditButton"), tr("编辑"));
    deleteButton_ = makeAction(QStringLiteral("stationDeleteButton"), tr("删除"));
    viewButton_ = makeAction(QStringLiteral("stationViewChargersButton"), tr("查看电桩"));
    batchButton_ = makeAction(QStringLiteral("stationBatchCreateButton"), tr("批量建桩"));
    actions->addStretch();
    layout->addWidget(actionBar);

    stateLabel_ = new QLabel(tr("尚未加载站点数据"), content);
    stateLabel_->setObjectName(QStringLiteral("stationStateLabel"));
    layout->addWidget(stateLabel_);
    ratingLabel_ = new QLabel(tr("请选择站点查看用户评价"), content);
    ratingLabel_->setObjectName(QStringLiteral("modalInfo"));
    ratingLabel_->setWordWrap(true);
    layout->addWidget(ratingLabel_);
    table_ = new QTableWidget(content);
    table_->setObjectName(QStringLiteral("stationTable"));
    table_->setColumnCount(7);
    table_->setHorizontalHeaderLabels({tr("站点"), tr("地址"), tr("电价"), tr("电桩数"),
                                       tr("空闲"), tr("经度"), tr("纬度")});
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setShowGrid(false);
    table_->verticalHeader()->setVisible(false);
    table_->verticalHeader()->setDefaultSectionSize(48);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    layout->addWidget(table_, 1);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(dashboard::scrollPage(content, this));
    connect(search_, &QLineEdit::textChanged, this, &StationPage::requestList);
    connect(refresh, &QPushButton::clicked, this, &StationPage::refresh);
    connect(add, &QPushButton::clicked, this, [this] { showStationDialog(false); });
    connect(editButton_, &QPushButton::clicked, this, [this] { showStationDialog(true); });
    connect(deleteButton_, &QPushButton::clicked, this, [this] {
        const qint64 id = selectedId();
        if (id <= 0 || !facade_) return;
        if (QMessageBox::question(this, tr("确认删除"),
                                  tr("站内仍有电桩时删除会失败，确认继续？")) == QMessageBox::Yes) {
            facade_->deleteStation(id);
        }
    });
    connect(viewButton_, &QPushButton::clicked, this, &StationPage::showChargers);
    connect(batchButton_, &QPushButton::clicked, this, &StationPage::showBatchDialog);
    connect(table_, &QTableWidget::itemSelectionChanged, this, &StationPage::updateActions);
}

void StationPage::bindFacade(AdminClientFacade &facade)
{
    facade_ = &facade;
    connect(&facade, &AdminClientFacade::stationsReceived, this, &StationPage::applyStations);
    connect(&facade, &AdminClientFacade::stationActionSucceeded, this,
            [this](const QString &, qint64) { requestList(); });
    connect(&facade, &AdminClientFacade::batchChargersSucceeded, this,
            [this](int count) {
                stateLabel_->setText(tr("已批量创建 %1 个电桩").arg(count));
                requestList();
            });
    connect(&facade, &AdminClientFacade::chargersReceived, this,
            [this](const QVector<Charger> &chargers) {
                if (!property("showingChargers").toBool()) return;
                QDialog dialog(this);
                auto *root = dashboard::modalLayout(&dialog, tr("站内电桩"),
                                                    tr("查看当前站点关联的充电设备"));
                auto *summary = new QLabel(
                    tr("站点：%1 · 设备数量：%2").arg(selectedStation().name).arg(chargers.size()),
                    &dialog);
                summary->setObjectName(QStringLiteral("modalInfo"));
                root->addWidget(summary);
                auto *table = new QTableWidget(chargers.size(), 4, &dialog);
                table->setObjectName(QStringLiteral("modalTable"));
                table->setHorizontalHeaderLabels({tr("编号"), tr("类型"), tr("功率"), tr("状态")});
                table->setSelectionMode(QAbstractItemView::NoSelection);
                table->setEditTriggers(QAbstractItemView::NoEditTriggers);
                table->setShowGrid(false);
                table->verticalHeader()->setVisible(false);
                table->verticalHeader()->setDefaultSectionSize(44);
                table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
                for (int row = 0; row < chargers.size(); ++row) {
                    const auto &charger = chargers.at(row);
                    table->setItem(row, 0, cell(charger.code));
                    table->setItem(row, 1, cell(chargerTypeText(charger.type)));
                    table->setItem(row, 2, cell(tr("%1 kW").arg(charger.powerKw, 0, 'f', 1)));
                    table->setItem(row, 3, cell(chargerStatusText(charger.status)));
                }
                root->addWidget(table, 1);
                auto *buttons = dashboard::modalButtons(&dialog, tr("关闭"));
                root->addWidget(buttons, 0, Qt::AlignRight);
                connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
                dialog.resize(620, 460);
                dialog.exec();
                setProperty("showingChargers", false);
            });
    connect(&facade, &AdminClientFacade::requestFailed, this,
            [this](const QString &route, int, const QString &message, int) {
                if (route.startsWith(QStringLiteral("admin.station."))) {
                    stateLabel_->setText(message);
                    stateLabel_->setProperty("state", "error");
                    stateLabel_->style()->unpolish(stateLabel_);
                    stateLabel_->style()->polish(stateLabel_);
                    updateActions();
                }
            });
}

void StationPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!loaded_) {
        loaded_ = true;
        requestList();
    }
}

void StationPage::requestList()
{
    if (!facade_) return;
    stateLabel_->setText(tr("正在加载站点列表…"));
    stateLabel_->setProperty("state", "loading");
    facade_->requestStations(search_->text());
    updateActions();
}

void StationPage::refresh()
{
    loaded_ = true;
    requestList();
}

void StationPage::applyStations(const QVector<Station> &stations)
{
    table_->setRowCount(0);
    for (const auto &station : stations) {
        const int row = table_->rowCount();
        table_->insertRow(row);
        auto *name = cell(station.name);
        name->setData(Qt::UserRole, station.id);
        name->setData(Qt::UserRole + 1, station.reviewCount);
        name->setData(Qt::UserRole + 2, station.averageScore);
        name->setData(Qt::UserRole + 3, station.environmentScore);
        name->setData(Qt::UserRole + 4, station.queueScore);
        name->setData(Qt::UserRole + 5, station.equipmentScore);
        name->setData(Qt::UserRole + 6, station.parkingScore);
        table_->setItem(row, 0, name);
        table_->setItem(row, 1, cell(station.address));
        table_->setItem(row, 2, cell(QString::number(station.price, 'f', 2)));
        table_->setItem(row, 3, cell(QString::number(station.chargerCount)));
        table_->setItem(row, 4, cell(QString::number(station.idleSlots)));
        table_->setItem(row, 5, cell(QString::number(station.longitude, 'f', 6)));
        table_->setItem(row, 6, cell(QString::number(station.latitude, 'f', 6)));
    }
    stateLabel_->setProperty("state", stations.isEmpty() ? "empty" : "normal");
    stateLabel_->setText(stations.isEmpty() ? tr("暂无符合条件的站点")
                                            : tr("共 %1 个站点").arg(stations.size()));
    stateLabel_->style()->unpolish(stateLabel_);
    stateLabel_->style()->polish(stateLabel_);
    updateActions();
    ratingLabel_->setText(tr("请选择站点查看用户评价"));
}

qint64 StationPage::selectedId() const
{
    const int row = table_->currentRow();
    return row >= 0 && !table_->selectedItems().isEmpty() && table_->item(row, 0)
        ? table_->item(row, 0)->data(Qt::UserRole).toLongLong() : 0;
}

Station StationPage::selectedStation() const
{
    Station station;
    station.id = selectedId();
    const int row = table_->currentRow();
    if (row < 0) return station;
    station.name = table_->item(row, 0)->text();
    station.address = table_->item(row, 1)->text();
    station.price = table_->item(row, 2)->text().toDouble();
    station.totalSlots = table_->item(row, 3)->text().toInt();
    station.longitude = table_->item(row, 5)->text().toDouble();
    station.latitude = table_->item(row, 6)->text().toDouble();
    const auto name = table_->item(row, 0);
    station.reviewCount = name->data(Qt::UserRole + 1).toLongLong();
    station.averageScore = name->data(Qt::UserRole + 2).toDouble();
    station.environmentScore = name->data(Qt::UserRole + 3).toDouble();
    station.queueScore = name->data(Qt::UserRole + 4).toDouble();
    station.equipmentScore = name->data(Qt::UserRole + 5).toDouble();
    station.parkingScore = name->data(Qt::UserRole + 6).toDouble();
    return station;
}

void StationPage::updateActions()
{
    const bool selected = selectedId() > 0;
    editButton_->setEnabled(selected);
    deleteButton_->setEnabled(selected);
    viewButton_->setEnabled(selected);
    batchButton_->setEnabled(selected);
    if (!selected) {
        ratingLabel_->setText(tr("请选择站点查看用户评价"));
        return;
    }
    const Station station = selectedStation();
    if (station.reviewCount == 0) {
        ratingLabel_->setText(tr("用户评价：暂无评价"));
        return;
    }
    QString insight;
    const StationRatingSummary summary{station.reviewCount, station.averageScore,
                                       station.environmentScore, station.queueScore,
                                       station.equipmentScore, station.parkingScore};
    for (const auto &item : stationRatingInsights(summary)) {
        if (item.level == QStringLiteral("warning")) {
            insight += QStringLiteral("\n⚠ ") + item.message;
        }
    }
    ratingLabel_->setText(tr("用户评价：综合 %1 · 环境 %2 · 排队 %3 · 设备 %4 · 停车 %5（%6 人）%7")
                          .arg(station.averageScore, 0, 'f', 1)
                          .arg(station.environmentScore, 0, 'f', 1)
                          .arg(station.queueScore, 0, 'f', 1)
                          .arg(station.equipmentScore, 0, 'f', 1)
                          .arg(station.parkingScore, 0, 'f', 1)
                          .arg(station.reviewCount).arg(insight));
}

void StationPage::showChargers()
{
    if (!facade_) return;
    setProperty("showingChargers", true);
    facade_->requestChargers({}, -1, selectedId());
}

} // namespace ncs
