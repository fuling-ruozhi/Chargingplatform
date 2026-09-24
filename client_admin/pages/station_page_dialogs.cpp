#include "station_page.h"

#include "../../core/service/admin_client_facade.h"
#include "dashboard_ui.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>

namespace ncs {

void StationPage::showStationDialog(bool edit)
{
    if (!facade_) return;
    Station station = edit ? selectedStation() : Station{};
    QDialog dialog(this);
    dialog.setWindowTitle(edit ? tr("编辑站点") : tr("新增站点"));
    auto *root = dashboard::modalLayout(&dialog,
        edit ? tr("编辑站点") : tr("新增站点"),
        edit ? tr("修改站点基础信息，保存后立即生效")
             : tr("创建新的充电站并配置基础运营信息"));
    auto *form = new QVBoxLayout;
    form->setSpacing(12);
    auto *name = new QLineEdit(station.name, &dialog);
    name->setProperty("role", "formInput");
    auto *address = new QLineEdit(station.address, &dialog);
    address->setProperty("role", "formInput");
    auto *longitude = new QDoubleSpinBox(&dialog);
    longitude->setProperty("role", "formInput");
    longitude->setRange(-180, 180);
    longitude->setDecimals(6);
    longitude->setValue(station.longitude);
    auto *latitude = new QDoubleSpinBox(&dialog);
    latitude->setProperty("role", "formInput");
    latitude->setRange(-90, 90);
    latitude->setDecimals(6);
    latitude->setValue(station.latitude);
    auto *price = new QDoubleSpinBox(&dialog);
    price->setProperty("role", "formInput");
    price->setRange(0.01, 100000);
    price->setValue(station.price > 0 ? station.price : 1);
    auto *slotsBox = new QSpinBox(&dialog);
    slotsBox->setProperty("role", "formInput");
    slotsBox->setRange(0, 100000);
    slotsBox->setValue(station.totalSlots);
    dashboard::addModalField(form, &dialog, tr("名称"), name);
    dashboard::addModalField(form, &dialog, tr("地址"), address);
    dashboard::addModalField(form, &dialog, tr("经度"), longitude);
    dashboard::addModalField(form, &dialog, tr("纬度"), latitude);
    dashboard::addModalField(form, &dialog, tr("电价"), price);
    dashboard::addModalField(form, &dialog, tr("电桩数"), slotsBox);
    root->addLayout(form);
    auto *error = new QLabel(&dialog);
    error->setObjectName(QStringLiteral("formError"));
    error->hide();
    root->addWidget(error);
    auto *buttons = dashboard::modalButtons(&dialog, edit ? tr("保存修改") : tr("新增站点"),
                                            tr("取消"));
    root->addWidget(buttons, 0, Qt::AlignRight);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        if (name->text().trimmed().isEmpty() || address->text().trimmed().isEmpty()) {
            error->setText(tr("名称和地址不能为空"));
            error->show();
            return;
        }
        error->hide();
        station.name = name->text();
        station.address = address->text();
        station.longitude = longitude->value();
        station.latitude = latitude->value();
        station.price = price->value();
        station.totalSlots = slotsBox->value();
        if (edit) facade_->updateStation(station);
        else facade_->createStation(station);
        dialog.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    dialog.exec();
}

void StationPage::showBatchDialog()
{
    if (!facade_) return;
    QDialog dialog(this);
    dialog.setWindowTitle(tr("批量建桩"));
    auto *root = dashboard::modalLayout(&dialog, tr("批量建桩"),
                                        tr("为当前站点一次创建多个充电桩"));
    auto *site = new QLabel(tr("当前站点：%1").arg(selectedStation().name), &dialog);
    site->setObjectName(QStringLiteral("modalInfo"));
    root->addWidget(site);
    auto *form = new QVBoxLayout;
    form->setSpacing(12);
    auto *prefix = new QLineEdit(&dialog);
    prefix->setProperty("role", "formInput");
    auto *count = new QSpinBox(&dialog);
    count->setProperty("role", "formInput");
    count->setRange(1, 100);
    auto *type = new QComboBox(&dialog);
    type->setProperty("role", "formInput");
    type->addItem(tr("交流慢充"), 0);
    type->addItem(tr("直流快充"), 1);
    auto *power = new QDoubleSpinBox(&dialog);
    power->setProperty("role", "formInput");
    power->setRange(0.1, 1000);
    power->setValue(7);
    dashboard::addModalField(form, &dialog, tr("编号前缀"), prefix);
    dashboard::addModalField(form, &dialog, tr("数量"), count);
    dashboard::addModalField(form, &dialog, tr("类型"), type);
    dashboard::addModalField(form, &dialog, tr("功率"), power);
    root->addLayout(form);
    auto *hint = new QLabel(tr("一次最多创建 100 个电桩"), &dialog);
    hint->setProperty("role", "modalSubtitle");
    root->addWidget(hint);
    auto *error = new QLabel(&dialog);
    error->setObjectName(QStringLiteral("formError"));
    error->hide();
    root->addWidget(error);
    auto *buttons = dashboard::modalButtons(&dialog, tr("批量创建"), tr("取消"));
    root->addWidget(buttons, 0, Qt::AlignRight);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        if (prefix->text().trimmed().isEmpty()) {
            error->setText(tr("编号前缀不能为空"));
            error->show();
            prefix->setFocus();
            return;
        }
        error->hide();
        facade_->batchCreateChargers(selectedId(), prefix->text(), count->value(),
                                     type->currentData().toInt(), power->value());
        dialog.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    dialog.exec();
}

}
