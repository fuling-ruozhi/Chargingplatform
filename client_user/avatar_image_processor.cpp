#include "avatar_image_processor.h"

#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QPainter>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>

namespace ncs {
namespace {

QString dataRoot()
{
    return QDir(QStandardPaths::writableLocation(
        QStandardPaths::GenericDataLocation)).filePath(QStringLiteral("NCS"));
}

QImage normalizedAvatar(const QImage &source)
{
    const int side = qMin(source.width(), source.height());
    const QRect crop((source.width() - side) / 2,
                     (source.height() - side) / 2, side, side);
    QImage square = source.copy(crop);
    if (square.width() > 512) {
        square = square.scaled(512, 512, Qt::KeepAspectRatio,
                               Qt::SmoothTransformation);
    }
    QImage output(square.size(), QImage::Format_RGB32);
    output.fill(Qt::white);
    QPainter painter(&output);
    painter.drawImage(0, 0, square);
    return output;
}

}

AvatarImageResult AvatarImageProcessor::process(const QString &sourcePath,
                                                qint64 userId)
{
    AvatarImageResult result;
    if (sourcePath.isEmpty() || !QFileInfo::exists(sourcePath)) {
        result.error = QStringLiteral("图片文件不存在，请重新选择");
        return result;
    }

    QImageReader reader(sourcePath);
    reader.setAutoTransform(true);
    const QImage source = reader.read();
    if (source.isNull()) {
        result.error = QStringLiteral("无法读取该图片，请选择 JPG、PNG 或 WebP 图片");
        return result;
    }

    QDir root(dataRoot());
    if (!root.mkpath(QStringLiteral("avatars"))) {
        result.error = QStringLiteral("无法创建头像目录，请检查文件权限");
        return result;
    }
    const QString fileName = QStringLiteral("%1-%2.jpg")
        .arg(userId)
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    result.relativePath = QStringLiteral("avatars/%1").arg(fileName);
    result.absolutePath = root.filePath(result.relativePath);

    QSaveFile target(result.absolutePath);
    if (!target.open(QIODevice::WriteOnly)
        || !normalizedAvatar(source).save(&target, "JPEG", 90)
        || !target.commit()) {
        target.cancelWriting();
        result.relativePath.clear();
        result.absolutePath.clear();
        result.error = QStringLiteral("头像保存失败，请检查磁盘空间和文件权限");
        return result;
    }
    result.success = true;
    return result;
}

QString AvatarImageProcessor::absolutePath(const QString &relativePath)
{
    return QDir(dataRoot()).filePath(relativePath);
}

}
