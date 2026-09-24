#pragma once

#include <QStringList>

class QFrame;
class QDialog;
class QDialogButtonBox;
class QLayout;
class QScrollArea;
class QVBoxLayout;
class QWidget;

namespace ncs::dashboard {
QFrame *card(QWidget *parent, const QString &objectName = QStringLiteral("card"));
QVBoxLayout *cardLayout(QFrame *card, int margin = 20, int spacing = 12);
void addCardHeader(QLayout *layout, const QString &title, const QString &subtitle = {});
QFrame *metricCard(QWidget *parent, const QString &title, const QString &value,
                   const QString &note, const QString &objectName = QStringLiteral("card"));
QFrame *tableCard(QWidget *parent, const QString &title, const QStringList &headers);
QScrollArea *scrollPage(QWidget *content, QWidget *parent);
QVBoxLayout *modalLayout(QDialog *dialog, const QString &title, const QString &subtitle);
void addModalField(QVBoxLayout *form, QWidget *parent, const QString &label, QWidget *field);
QDialogButtonBox *modalButtons(QDialog *dialog, const QString &primaryText,
                               const QString &secondaryText = QString());
} // namespace ncs::dashboard
