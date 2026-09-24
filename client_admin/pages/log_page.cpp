#include "log_page.h"
#include "service/admin_client_facade.h"
#include "util/logger.h"
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTabWidget>
#include <QTextStream>
#include <QTextEdit>
#include <QVBoxLayout>

namespace ncs {
LogPage::LogPage(QWidget *parent) : QWidget(parent) {
    auto *root = new QVBoxLayout(this); auto *filters = new QHBoxLayout;
    keyword_ = new QLineEdit(this); keyword_->setPlaceholderText(tr("关键词"));
    type_ = new QComboBox(this); type_->addItems({tr("全部类型"), "AUTH", "CHARGER", "STATION", "USER", "SYSTEM"});
    result_ = new QComboBox(this); result_->addItems({tr("全部结果"), "SUCCESS", "FAILED", "LOCKED"});
    auto *refreshButton = new QPushButton(tr("刷新"), this); filters->addWidget(keyword_); filters->addWidget(type_); filters->addWidget(result_); filters->addWidget(refreshButton); root->addLayout(filters);
    tabs_ = new QTabWidget(this); login_ = new QTableWidget(this); operation_ = new QTableWidget(this); security_ = new QTableWidget(this); system_ = new QTextEdit(this); system_->setReadOnly(true);
    tabs_->addTab(login_, tr("登录日志")); tabs_->addTab(operation_, tr("操作日志")); tabs_->addTab(security_, tr("安全日志")); tabs_->addTab(system_, tr("系统日志")); root->addWidget(tabs_);
    for (auto *table : {login_, operation_, security_}) { table->setEditTriggers(QAbstractItemView::NoEditTriggers); table->horizontalHeader()->setStretchLastSection(true); table->setAlternatingRowColors(true); }
    connect(refreshButton, &QPushButton::clicked, this, &LogPage::refresh); connect(tabs_, &QTabWidget::currentChanged, this, [this] { requestCurrent(); });
}
void LogPage::bindFacade(AdminClientFacade &facade) { facade_ = &facade; connect(&facade, &AdminClientFacade::logsReceived, this, [this](const QString &kind, const QJsonArray &items) { if (kind == "login") fill(login_, items, {"时间","管理员","IP","结果","原因","失败次数"}); else if (kind == "operation") fill(operation_, items, {"时间","管理员","模块","操作","对象类型","对象ID","详情","结果","错误"}); else fill(security_, items, {"时间","等级","事件","用户/管理员","IP","描述"}); }); refresh(); }
void LogPage::requestCurrent() { if (!facade_) return; const int i = tabs_->currentIndex(); if (i == 3) { QFile file(QDir(Logger::logDirectory()).filePath("app.log")); if (file.open(QIODevice::ReadOnly | QIODevice::Text)) { QTextStream s(&file); system_->setPlainText(s.readAll()); } return; } if (i == 0) facade_->requestLogs("login", keyword_->text(), QString(), result_->currentIndex() ? result_->currentText() : QString()); else if (i == 1) facade_->requestLogs("operation", keyword_->text(), type_->currentIndex() ? type_->currentText() : QString(), result_->currentIndex() ? result_->currentText() : QString()); else facade_->requestLogs("security", QString(), type_->currentIndex() ? type_->currentText() : QString(), result_->currentIndex() ? result_->currentText() : QString()); }
void LogPage::refresh() { requestCurrent(); }
void LogPage::fill(QTableWidget *table, const QJsonArray &items, const QStringList &columns) { table->clear(); table->setColumnCount(columns.size()); table->setHorizontalHeaderLabels(columns); table->setRowCount(items.size()); for (int r=0;r<items.size();++r) { const auto o=items[r].toObject(); const QStringList keys = table == login_ ? QStringList{"time","username","ip","result","reason","failed_attempts"} : table == operation_ ? QStringList{"time","username","module","action","target_type","target_id","detail","result","error"} : QStringList{"time","severity","event_type","username","ip","description"}; for (int c=0;c<keys.size();++c) table->setItem(r,c,new QTableWidgetItem(o.value(keys[c]).toVariant().toString())); } }
}
