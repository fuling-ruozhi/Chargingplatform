#include "confirm_action_dialog.h"

#include <QApplication>
#include <QLabel>
#include <QPushButton>

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    ncs::ConfirmActionDialog cancelDialog(
        QStringLiteral("取消测试"), QStringLiteral("是否继续？"),
        QStringLiteral("确认"), ncs::ConfirmActionDialog::Severity::Warning);
    int cancelledConfirmations = 0;
    QObject::connect(&cancelDialog, &ncs::ConfirmActionDialog::confirmed,
                     [&] { ++cancelledConfirmations; });
    auto *cancel = cancelDialog.findChild<QPushButton *>(
        QStringLiteral("confirmCancelButton"));
    if (!cancel) return 1;
    cancelDialog.show();
    cancel->click();
    if (cancelledConfirmations != 0 || cancelDialog.isVisible()) return 2;

    ncs::ConfirmActionDialog dialog(
        QStringLiteral("异步操作"), QStringLiteral("高风险操作需要确认"),
        QStringLiteral("继续"), ncs::ConfirmActionDialog::Severity::Danger);
    auto *confirm = dialog.findChild<QPushButton *>(
        QStringLiteral("confirmSubmitButton"));
    cancel = dialog.findChild<QPushButton *>(QStringLiteral("confirmCancelButton"));
    if (!confirm || !cancel) return 3;
    int confirmationCount = 0;
    QObject::connect(&dialog, &ncs::ConfirmActionDialog::confirmed,
                     [&] { ++confirmationCount; });
    dialog.show();
    confirm->click();
    confirm->click();
    dialog.reject();
    if (confirmationCount != 1 || !dialog.isPending() || confirm->isEnabled()
        || cancel->isEnabled() || !dialog.isVisible())
        return 4;

    dialog.finishFailure(QStringLiteral("请求超时，请重试"));
    if (dialog.isPending() || !confirm->isEnabled() || !cancel->isEnabled())
        return 5;
    confirm->click();
    if (confirmationCount != 2 || !dialog.isPending()) return 6;
    dialog.finishSuccess();
    if (dialog.isVisible()) return 7;

    ncs::ConfirmActionDialog destroyedReceiver(
        QStringLiteral("生命周期"), QStringLiteral("接收者已销毁"),
        QStringLiteral("确认"), ncs::ConfirmActionDialog::Severity::Warning);
    auto *receiver = new QObject;
    int destroyedCount = 0;
    QObject::connect(&destroyedReceiver, &ncs::ConfirmActionDialog::confirmed,
                     receiver, [&] { ++destroyedCount; });
    delete receiver;
    confirm = destroyedReceiver.findChild<QPushButton *>(
        QStringLiteral("confirmSubmitButton"));
    confirm->click();
    if (destroyedCount != 0) return 8;
    return 0;
}
