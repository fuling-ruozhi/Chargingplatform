#pragma once

#include <QDialog>

class QLabel;
class QPushButton;

namespace ncs {

class ConfirmActionDialog : public QDialog
{
    Q_OBJECT
public:
    enum class Severity { Warning, Danger };

    ConfirmActionDialog(const QString &title, const QString &message,
                        const QString &confirmText, Severity severity,
                        QWidget *parent = nullptr);

    bool isPending() const;

public slots:
    void finishSuccess();
    void finishFailure(const QString &message);
    void reject() override;

signals:
    void confirmed();

private:
    QLabel *statusLabel_ = nullptr;
    QPushButton *confirmButton_ = nullptr;
    QPushButton *cancelButton_ = nullptr;
    bool pending_ = false;
};

}
