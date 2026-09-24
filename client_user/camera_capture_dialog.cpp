#include "camera_capture_dialog.h"

#include <QCamera>
#include <QDir>
#include <QHBoxLayout>
#include <QImageCapture>
#include <QLabel>
#include <QMediaCaptureSession>
#include <QPixmap>
#include <QPushButton>
#include <QShowEvent>
#include <QStyle>
#include <QVBoxLayout>
#include <QVideoWidget>

namespace ncs {

CameraCaptureDialog::CameraCaptureDialog(const QCameraDevice &device,
                                         QWidget *parent)
    : QDialog(parent), session_(new QMediaCaptureSession(this)),
      camera_(new QCamera(device, this)), capture_(new QImageCapture(this)),
      videoWidget_(new QVideoWidget(this)), previewLabel_(new QLabel(this)),
      statusLabel_(new QLabel(this)),
      captureButton_(new QPushButton(QStringLiteral("拍照"), this)),
      retakeButton_(new QPushButton(QStringLiteral("重新拍摄"), this)),
      useButton_(new QPushButton(QStringLiteral("使用照片"), this))
{
    setWindowTitle(QStringLiteral("拍摄头像"));
    setModal(true);
    setFixedSize(460, 610);
    setObjectName(QStringLiteral("cameraCaptureDialog"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(12);
    auto *title = new QLabel(QStringLiteral("拍摄头像"), this);
    title->setObjectName(QStringLiteral("pageTitle"));
    layout->addWidget(title);

    videoWidget_->setObjectName(QStringLiteral("cameraVideoWidget"));
    videoWidget_->setMinimumSize(424, 424);
    videoWidget_->setAspectRatioMode(Qt::KeepAspectRatioByExpanding);
    previewLabel_->setObjectName(QStringLiteral("cameraPhotoPreview"));
    previewLabel_->setMinimumSize(424, 424);
    previewLabel_->setAlignment(Qt::AlignCenter);
    previewLabel_->setScaledContents(false);
    previewLabel_->hide();
    layout->addWidget(videoWidget_, 1);
    layout->addWidget(previewLabel_, 1);

    auto *hint = new QLabel(QStringLiteral("请将头像区域置于取景框中央"), this);
    hint->setObjectName(QStringLiteral("mutedLabel"));
    hint->setAlignment(Qt::AlignCenter);
    layout->addWidget(hint);
    statusLabel_->setObjectName(QStringLiteral("statusLoading"));
    statusLabel_->setAlignment(Qt::AlignCenter);
    statusLabel_->setWordWrap(true);
    statusLabel_->hide();
    layout->addWidget(statusLabel_);

    auto *previewActions = new QHBoxLayout;
    retakeButton_->setProperty("variant", QStringLiteral("secondary"));
    retakeButton_->setObjectName(QStringLiteral("cameraRetakeButton"));
    useButton_->setObjectName(QStringLiteral("cameraUseButton"));
    previewActions->addWidget(retakeButton_);
    previewActions->addWidget(useButton_);
    retakeButton_->hide();
    useButton_->hide();
    captureButton_->setObjectName(QStringLiteral("cameraCaptureButton"));
    layout->addWidget(captureButton_);
    layout->addLayout(previewActions);

    session_->setCamera(camera_);
    session_->setImageCapture(capture_);
    session_->setVideoOutput(videoWidget_);

    connect(captureButton_, &QPushButton::clicked,
            this, &CameraCaptureDialog::capture);
    connect(retakeButton_, &QPushButton::clicked,
            this, &CameraCaptureDialog::retake);
    connect(useButton_, &QPushButton::clicked,
            this, &CameraCaptureDialog::usePhoto);
    connect(capture_, &QImageCapture::readyForCaptureChanged,
            this, [this](bool ready) {
                if (!capturePending_ && !previewReady_)
                    captureButton_->setEnabled(ready);
            });
    connect(capture_, &QImageCapture::imageCaptured,
            this, [this](int id, const QImage &image) {
                if (id != pendingCaptureId_) return;
                showPreview(image);
            });
    connect(capture_, &QImageCapture::imageSaved,
            this, [this](int id, const QString &path) {
                if (id != pendingCaptureId_) return;
                capturePending_ = false;
                capturedPath_ = path;
                useButton_->setEnabled(true);
                setStatus(QStringLiteral("照片已准备好，请确认是否使用"), false);
            });
    connect(capture_, &QImageCapture::errorOccurred,
            this, [this](int id, QImageCapture::Error, const QString &message) {
                if (id != pendingCaptureId_ && id >= 0) return;
                capturePending_ = false;
                pendingCaptureId_ = -1;
                capturedPath_.clear();
                showCameraError(message);
            });
    connect(camera_, &QCamera::errorOccurred,
            this, [this](QCamera::Error, const QString &message) {
                capturePending_ = false;
                showCameraError(message);
            });
}

CameraCaptureDialog::~CameraCaptureDialog()
{
    stopCamera();
}

void CameraCaptureDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    if (!previewReady_ && camera_->cameraDevice().isNull() == false)
        camera_->start();
}

void CameraCaptureDialog::done(int result)
{
    capturePending_ = false;
    pendingCaptureId_ = -1;
    stopCamera();
    QDialog::done(result);
}

void CameraCaptureDialog::capture()
{
    if (capturePending_ || previewReady_) return;
    if (!capture_->isReadyForCapture()) {
        showCameraError(QStringLiteral("摄像头尚未准备好或正被其他程序使用"));
        return;
    }
    if (!temporaryDir_.isValid()) {
        showCameraError(QStringLiteral("无法创建临时照片目录"));
        return;
    }
    capturePending_ = true;
    captureButton_->setEnabled(false);
    setStatus(QStringLiteral("正在拍摄，请稍候…"), false);
    const QString path = QDir(temporaryDir_.path()).filePath(
        QStringLiteral("captured-avatar.jpg"));
    pendingCaptureId_ = capture_->captureToFile(path);
    if (pendingCaptureId_ < 0) {
        capturePending_ = false;
        showCameraError(QStringLiteral("无法启动拍照，请重试"));
    }
}

void CameraCaptureDialog::showPreview(const QImage &image)
{
    if (!capturePending_) return;
    if (image.isNull()) {
        showCameraError(QStringLiteral("摄像头返回了无效照片"));
        return;
    }
    previewReady_ = true;
    stopCamera();
    previewLabel_->setPixmap(QPixmap::fromImage(image).scaled(
        previewLabel_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    videoWidget_->hide();
    previewLabel_->show();
    captureButton_->hide();
    retakeButton_->show();
    useButton_->show();
    useButton_->setEnabled(false);
}

void CameraCaptureDialog::retake()
{
    if (capturePending_) return;
    previewReady_ = false;
    pendingCaptureId_ = -1;
    capturedPath_.clear();
    previewLabel_->clear();
    previewLabel_->hide();
    videoWidget_->show();
    captureButton_->show();
    retakeButton_->hide();
    useButton_->hide();
    statusLabel_->hide();
    camera_->start();
    captureButton_->setEnabled(capture_->isReadyForCapture());
}

void CameraCaptureDialog::usePhoto()
{
    if (capturePending_ || capturedPath_.isEmpty()) return;
    emit photoSelected(capturedPath_);
    accept();
}

void CameraCaptureDialog::showCameraError(const QString &detail)
{
    stopCamera();
    captureButton_->setEnabled(true);
    retakeButton_->setEnabled(true);
    useButton_->setEnabled(false);
    const QString reason = detail.trimmed().isEmpty()
        ? QStringLiteral("摄像头初始化失败") : detail.trimmed();
    setStatus(QStringLiteral("无法访问摄像头：%1。请检查系统摄像头权限，或关闭占用摄像头的程序。")
                  .arg(reason), true);
}

void CameraCaptureDialog::setStatus(const QString &text, bool error)
{
    statusLabel_->setObjectName(error ? QStringLiteral("statusError")
                                      : QStringLiteral("statusLoading"));
    statusLabel_->style()->unpolish(statusLabel_);
    statusLabel_->style()->polish(statusLabel_);
    statusLabel_->setText(text);
    statusLabel_->show();
}

void CameraCaptureDialog::stopCamera()
{
    if (camera_ && camera_->isActive()) camera_->stop();
}

}
