#include "avatar_source_dialog.h"

#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace ncs {
namespace {

QLabel *heading(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("pageTitle"));
    return label;
}

QPushButton *sourceButton(const QString &text, const QString &detail,
                          QWidget *parent)
{
    auto *button = new QPushButton(
        QStringLiteral("%1\n%2").arg(text, detail), parent);
    button->setProperty("variant", QStringLiteral("plan-card"));
    button->setMinimumHeight(68);
    return button;
}

}

AvatarSourceDialog::AvatarSourceDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(QStringLiteral("修改头像"));
    setModal(true);
    setFixedSize(360, 330);
    setObjectName(QStringLiteral("avatarSourceDialog"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(22, 22, 22, 22);
    layout->setSpacing(12);
    layout->addWidget(heading(QStringLiteral("修改头像"), this));
    auto *hint = new QLabel(QStringLiteral("选择一张清晰照片作为你的新头像"), this);
    hint->setObjectName(QStringLiteral("mutedLabel"));
    layout->addWidget(hint);

    auto *album = sourceButton(QStringLiteral("从相册选择"),
                               QStringLiteral("支持 JPG、PNG、WebP"), this);
    album->setObjectName(QStringLiteral("avatarAlbumButton"));
    auto *camera = sourceButton(QStringLiteral("拍照"),
                                QStringLiteral("使用当前设备摄像头"), this);
    camera->setObjectName(QStringLiteral("avatarCameraButton"));
    auto *cancel = new QPushButton(QStringLiteral("取消"), this);
    cancel->setProperty("variant", QStringLiteral("ghost"));
    cancel->setObjectName(QStringLiteral("avatarSourceCancelButton"));
    layout->addWidget(album);
    layout->addWidget(camera);
    layout->addStretch();
    layout->addWidget(cancel);

    connect(album, &QPushButton::clicked, this, [this] {
        emit albumRequested();
        accept();
    });
    connect(camera, &QPushButton::clicked, this, [this] {
        emit cameraRequested();
        accept();
    });
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
}

CameraUnavailableDialog::CameraUnavailableDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("摄像头不可用"));
    setModal(true);
    setFixedSize(360, 245);
    setObjectName(QStringLiteral("cameraUnavailableDialog"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(22, 22, 22, 22);
    layout->setSpacing(12);
    layout->addWidget(heading(QStringLiteral("未检测到可用摄像头"), this));
    auto *message = new QLabel(
        QStringLiteral("可以从相册选择头像。如果设备有摄像头，请检查系统摄像头权限。"),
        this);
    message->setObjectName(QStringLiteral("mutedLabel"));
    message->setWordWrap(true);
    layout->addWidget(message);
    layout->addStretch();
    auto *album = new QPushButton(QStringLiteral("从相册选择"), this);
    album->setObjectName(QStringLiteral("cameraFallbackAlbumButton"));
    auto *close = new QPushButton(QStringLiteral("关闭"), this);
    close->setProperty("variant", QStringLiteral("ghost"));
    close->setObjectName(QStringLiteral("cameraFallbackCloseButton"));
    layout->addWidget(album);
    layout->addWidget(close);
    connect(album, &QPushButton::clicked, this, [this] {
        emit albumRequested();
        accept();
    });
    connect(close, &QPushButton::clicked, this, &QDialog::reject);
}

}
