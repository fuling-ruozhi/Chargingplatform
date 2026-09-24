#include "date_time_storage.h"

namespace ncs {
namespace {

const QString StorageFormat = QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz");

}

QDateTime DateTimeStorage::now()
{
    return QDateTime::currentDateTime();
}

QString DateTimeStorage::nowText()
{
    return toText(now());
}

QString DateTimeStorage::toText(const QDateTime &value)
{
    return value.toLocalTime().toString(StorageFormat);
}

QDateTime DateTimeStorage::fromText(const QString &value)
{
    QDateTime result = QDateTime::fromString(value, StorageFormat);
    if (!result.isValid()) result = QDateTime::fromString(value, Qt::ISODateWithMs);
    if (!result.isValid()) result = QDateTime::fromString(value, Qt::ISODate);
    if (!result.isValid()) {
        result = QDateTime::fromString(value, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    }
    return result;
}

}
