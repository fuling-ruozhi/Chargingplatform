#pragma once
#include <QWidget>
#include <QJsonArray>
#include <QStringList>
class QLineEdit; class QComboBox; class QTabWidget; class QTableWidget; class QTextEdit;
namespace ncs { class AdminClientFacade;
class LogPage : public QWidget {
    Q_OBJECT
public: explicit LogPage(QWidget *parent = nullptr); void bindFacade(AdminClientFacade &facade); void refresh();
private: void requestCurrent(); void fill(QTableWidget *table, const QJsonArray &items, const QStringList &columns);
    AdminClientFacade *facade_ = nullptr; QTabWidget *tabs_ = nullptr; QLineEdit *keyword_ = nullptr; QComboBox *type_ = nullptr; QComboBox *result_ = nullptr; QTableWidget *login_ = nullptr; QTableWidget *operation_ = nullptr; QTableWidget *security_ = nullptr; QTextEdit *system_ = nullptr;
}; }
