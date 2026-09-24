#include "password_hasher.h"

#include <QCryptographicHash>
#include <QtGlobal>
#include <QUuid>

namespace ncs {

QString PasswordHasher::makeSalt()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString PasswordHasher::hash(const QString &password, const QString &salt)
{
    return QString::fromLatin1(QCryptographicHash::hash(
        salt.toUtf8() + password.toUtf8(), QCryptographicHash::Sha256).toHex());
}

bool PasswordHasher::verify(const QString &password, const QString &salt,
                            const QString &expectedHash)
{
    const QByteArray actual = hash(password, salt).toLatin1();
    const QByteArray expected = expectedHash.toLatin1();
    const int maxSize = qMax(actual.size(), expected.size());
    uchar diff = static_cast<uchar>(actual.size() ^ expected.size());
    for (int i = 0; i < maxSize; ++i) {
        const uchar left = i < actual.size() ? static_cast<uchar>(actual.at(i)) : 0;
        const uchar right = i < expected.size() ? static_cast<uchar>(expected.at(i)) : 0;
        diff = static_cast<uchar>(diff | (left ^ right));
    }
    return diff == 0;
}

}
