#pragma once

#include <QCameraDevice>
#include <QDialog>
#include <QTemporaryDir>

class QLabel;
class QPushButton;
class QCamera;
class QImageCapture;
class QMediaCaptureSession;
class QVideoWidget;

namespace ncs {

class CameraCaptureDialog : public QDialog
{
    Q_OBJECT
public:
    explicit CameraCaptureDialog(const QCameraDevice &device,
                                 QWidget *parent = nullptr);
    ~CameraCaptureDialog() override;

signals:
    void photoSelected(const QString &path);

protected:
    void showEvent(QShowEvent *event) override;
    void done(int result) override;

private:
    void capture();
    void showPreview(const QImage &image);
    void retake();
    void usePhoto();
    void showCameraError(const QString &detail);
    void setStatus(const QString &text, bool error);
    void stopCamera();

    QTemporaryDir temporaryDir_;
    QMediaCaptureSession *session_ = nullptr;
    QCamera *camera_ = nullptr;
    QImageCapture *capture_ = nullptr;
    QVideoWidget *videoWidget_ = nullptr;
    QLabel *previewLabel_ = nullptr;
    QLabel *statusLabel_ = nullptr;
    QPushButton *captureButton_ = nullptr;
    QPushButton *retakeButton_ = nullptr;
    QPushButton *useButton_ = nullptr;
    bool capturePending_ = false;
    bool previewReady_ = false;
    int pendingCaptureId_ = -1;
    QString capturedPath_;
};

}
