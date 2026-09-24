#include "avatar_image_processor.h"

#include <QApplication>
#include <QColor>
#include <QFile>
#include <QImage>
#include <QStandardPaths>
#include <QTemporaryDir>

namespace {

bool verifyImage(const QString &sourcePath, qint64 userId,
                 const QSize &expectedSize)
{
    const ncs::AvatarImageResult result =
        ncs::AvatarImageProcessor::process(sourcePath, userId);
    if (!result.success || !result.relativePath.startsWith(QStringLiteral("avatars/"))
        || !result.relativePath.endsWith(QStringLiteral(".jpg"))) return false;
    const QImage output(result.absolutePath);
    const bool valid = !output.isNull() && output.size() == expectedSize
        && output.width() == output.height();
    QFile::remove(result.absolutePath);
    return valid;
}

bool createImage(const QString &path, const QSize &size, const QColor &color)
{
    QImage image(size, QImage::Format_RGB32);
    image.fill(color);
    return image.save(path);
}

}

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    QTemporaryDir directory;
    if (!directory.isValid()) return 1;

    const QString landscape = directory.filePath(QStringLiteral("landscape.png"));
    const QString portrait = directory.filePath(QStringLiteral("portrait.png"));
    const QString square = directory.filePath(QStringLiteral("square.png"));
    const QString large = directory.filePath(QStringLiteral("large.png"));
    if (!createImage(landscape, QSize(800, 400), Qt::red)
        || !createImage(portrait, QSize(300, 900), Qt::green)
        || !createImage(square, QSize(256, 256), Qt::blue)
        || !createImage(large, QSize(2000, 1000), Qt::yellow)) return 2;

    if (!verifyImage(landscape, 101, QSize(400, 400))) return 3;
    if (!verifyImage(portrait, 102, QSize(300, 300))) return 4;
    if (!verifyImage(square, 103, QSize(256, 256))) return 5;
    if (!verifyImage(large, 104, QSize(512, 512))) return 6;

    const QString invalid = directory.filePath(QStringLiteral("invalid.jpg"));
    QFile badFile(invalid);
    if (!badFile.open(QIODevice::WriteOnly)
        || badFile.write("not an image") < 0) return 7;
    badFile.close();
    const auto invalidResult = ncs::AvatarImageProcessor::process(invalid, 105);
    if (invalidResult.success || invalidResult.error.isEmpty()) return 8;
    return 0;
}
