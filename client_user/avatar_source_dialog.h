#pragma once

#include <QDialog>

namespace ncs {

class AvatarSourceDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AvatarSourceDialog(QWidget *parent = nullptr);

signals:
    void albumRequested();
    void cameraRequested();
};

class CameraUnavailableDialog : public QDialog
{
    Q_OBJECT
public:
    explicit CameraUnavailableDialog(QWidget *parent = nullptr);

signals:
    void albumRequested();
};

}
