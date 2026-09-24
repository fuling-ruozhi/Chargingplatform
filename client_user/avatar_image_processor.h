#pragma once

#include <QString>

namespace ncs {

struct AvatarImageResult
{
    bool success = false;
    QString relativePath;
    QString absolutePath;
    QString error;
};

class AvatarImageProcessor
{
public:
    static AvatarImageResult process(const QString &sourcePath, qint64 userId);
    static QString absolutePath(const QString &relativePath);
};

}
