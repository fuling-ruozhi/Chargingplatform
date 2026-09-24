#include "confirm_action_dialog.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

namespace ncs {

ConfirmActionDialog::ConfirmActionDialog(
    const QString &title, const QString &message, const QString &confirmText,
    Severity severity, QWidget *parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("confirmActionDialog"));
    setWindowTitle(title);
    setModal(true);
    setFixedWidth(360);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 18);
    layout->setSpacing(12);
    auto *heading = new QLabel(title, this);
    heading->setObjectName(QStringLiteral("sectionTitle"));
    auto *body = new QLabel(message, this);
    body->setObjectName(QStringLiteral("mutedLabel"));
    body->setWordWrap(true);
    statusLabel_ = new QLabel(this);
    statusLabel_->setObjectName(QStringLiteral("statusLoading"));
    statusLabel_->setWordWrap(true);
    statusLabel_->hide();
    auto *buttons = new QHBoxLayout;
    cancelButton_ = new QPushButton(QStringLiteral("返回"), this);
    cancelButton_->setObjectName(QStringLiteral("confirmCancelButton"));
    confirmButton_ = new QPushButton(confirmText, this);
    confirmButton_->setObjectName(QStringLiteral("confirmSubmitButton"));
    if (severity == Severity::Danger) {
        confirmButton_->setProperty("danger", true);
    }
    buttons->addWidget(cancelButton_);
    buttons->addWidget(confirmButton_);
    layout->addWidget(heading);
    layout->addWidget(body);
    layout->addWidget(statusLabel_);
    layout->addLayout(buttons);

    connect(cancelButton_, &QPushButton::clicked,
            this, &ConfirmActionDialog::reject);
    connect(confirmButton_, &QPushButton::clicked, this, [this] {
        if (pending_) return;
        pending_ = true;
        confirmButton_->setEnabled(false);
        cancelButton_->setEnabled(false);
        statusLabel_->setObjectName(QStringLiteral("statusLoading"));
        statusLabel_->setText(QStringLiteral("正在处理，请稍候…"));
        statusLabel_->show();
        emit confirmed();
    });
}

bool ConfirmActionDialog::isPending() const
{
    return pending_;
}

void ConfirmActionDialog::finishSuccess()
{
    pending_ = false;
    accept();
}

void ConfirmActionDialog::finishFailure(const QString &message)
{
    pending_ = false;
    confirmButton_->setEnabled(true);
    cancelButton_->setEnabled(true);
    statusLabel_->setObjectName(QStringLiteral("statusError"));
    statusLabel_->setText(message);
    statusLabel_->style()->unpolish(statusLabel_);
    statusLabel_->style()->polish(statusLabel_);
    statusLabel_->show();
}

void ConfirmActionDialog::reject()
{
    if (!pending_) QDialog::reject();
}

}
