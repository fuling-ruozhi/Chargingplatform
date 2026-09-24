#include "avatar_source_dialog.h"
#include "user_profile_dialog.h"
#include "service/user_client_facade.h"

#include <QApplication>
#include <QCameraDevice>
#include <QDir>
#include <QImage>
#include <QLabel>
#include <QMetaObject>
#include <QPushButton>
#include <QStandardPaths>
#include <QTemporaryDir>

namespace {

QPushButton *buttonByObject(QWidget &root, const QString &name)
{
    return root.findChild<QPushButton *>(name);
}

QPushButton *buttonByText(QWidget &root, const QString &text)
{
    for (QPushButton *button : root.findChildren<QPushButton *>()) {
        if (button->text() == text) return button;
    }
    return nullptr;
}

bool hasText(QWidget &root, const QString &text)
{
    for (QLabel *label : root.findChildren<QLabel *>()) {
        if (label->text().contains(text)) return true;
    }
    return false;
}

void settleEvents()
{
    for (int i = 0; i < 8; ++i)
        QApplication::processEvents(QEventLoop::AllEvents, 20);
}

}

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);

    ncs::User user;
    user.id = 9001;
    user.phone = QStringLiteral("13800138000");
    user.nickname = QStringLiteral("旧头像");
    ncs::UserClientFacade facade;
    ncs::UserProfileDialog profile(
        user, facade, nullptr, [] { return QList<QCameraDevice>(); });
    profile.show();
    settleEvents();

    auto *modify = buttonByText(profile, QStringLiteral("修改头像"));
    if (!modify) return 1;
    modify->click();
    settleEvents();
    auto *source = profile.findChild<ncs::AvatarSourceDialog *>();
    if (!source || !source->isVisible()
        || !buttonByObject(*source, QStringLiteral("avatarAlbumButton"))
        || !buttonByObject(*source, QStringLiteral("avatarCameraButton"))) return 2;

    buttonByObject(*source, QStringLiteral("avatarCameraButton"))->click();
    settleEvents();
    auto *fallback = profile.findChild<ncs::CameraUnavailableDialog *>();
    if (!fallback || !fallback->isVisible()
        || !hasText(*fallback, QStringLiteral("未检测到可用摄像头"))
        || !buttonByObject(*fallback, QStringLiteral("cameraFallbackAlbumButton"))
        || !buttonByObject(*fallback, QStringLiteral("cameraFallbackCloseButton"))) return 3;
    buttonByObject(*fallback, QStringLiteral("cameraFallbackCloseButton"))->click();
    settleEvents();

    QTemporaryDir directory;
    const QString imagePath = directory.filePath(QStringLiteral("avatar.png"));
    QImage image(720, 480, QImage::Format_RGB32);
    image.fill(Qt::magenta);
    if (!image.save(imagePath)) return 4;

    const QString avatarDir = QDir(QStandardPaths::writableLocation(
        QStandardPaths::GenericDataLocation)).filePath(QStringLiteral("NCS/avatars"));
    const int before = QDir(avatarDir).entryList(
        QStringList{QStringLiteral("*.jpg")}, QDir::Files).size();
    bool changed = false;
    QObject::connect(&profile, &ncs::UserProfileDialog::userChanged,
                     [&] { changed = true; });
    if (!QMetaObject::invokeMethod(&profile, "applyAvatarFromFile",
                                   Qt::DirectConnection,
                                   Q_ARG(QString, imagePath))) return 5;
    settleEvents();
    const int after = QDir(avatarDir).entryList(
        QStringList{QStringLiteral("*.jpg")}, QDir::Files).size();
    if (changed || after != before
        || !hasText(profile, QStringLiteral("登录状态已失效"))) return 6;

    profile.close();
    return 0;
}
