#pragma once

#include <QSqlDatabase>
#include <QStringList>

namespace ncs {

bool requireColumns(QSqlDatabase &database, const QString &table,
                    const QStringList &required, QString *error);

}
