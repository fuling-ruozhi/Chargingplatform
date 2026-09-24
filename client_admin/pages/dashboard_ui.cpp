#include "dashboard_ui.h"

#include "../icon_assets.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QIcon>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace ncs::dashboard {

QFrame *card(QWidget *parent, const QString &name)
{
    auto *result = new QFrame(parent);
    result->setObjectName(name);
    result->setFrameShape(QFrame::NoFrame);
    return result;
}

QVBoxLayout *cardLayout(QFrame *target, int margin, int spacing)
{
    auto *layout = new QVBoxLayout(target);
    layout->setContentsMargins(margin, margin, margin, margin);
    layout->setSpacing(spacing);
    return layout;
}

void addCardHeader(QLayout *layout, const QString &title, const QString &subtitle)
{
    auto *heading = new QLabel(title);
    heading->setObjectName("cardTitle");
    layout->addWidget(heading);

    if (!subtitle.isEmpty()) {
        auto *note = new QLabel(subtitle);
        note->setObjectName("secondaryText");
        layout->addWidget(note);
    }
}

QFrame *metricCard(QWidget *parent, const QString &title, const QString &value,
                  const QString &note, const QString &name)
{
    auto *result = card(parent, name);
    result->setMinimumHeight(104);
    auto *layout = cardLayout(result, 16, 4);

    auto *heading = new QLabel(title, result);
    heading->setObjectName("metricTitle");
    auto *number = new QLabel(value, result);
    number->setObjectName("metricValue");
    auto *detail = new QLabel(note, result);
    detail->setObjectName("metricNote");
    layout->addWidget(heading);
    layout->addWidget(number);
    layout->addStretch();
    layout->addWidget(detail);

    auto *track = new QProgressBar(result);
    track->setObjectName("metricTrack");
    track->setFixedHeight(4);
    track->setRange(0, 100);
    track->setValue(0);
    track->setTextVisible(false);
    layout->addWidget(track);
    return result;
}

QFrame *tableCard(QWidget *parent, const QString &title, const QStringList &headers)
{
    auto *result = card(parent, "tableCard");
    auto *layout = cardLayout(result);
    addCardHeader(layout, title);

    auto *header = new QFrame(result);
    header->setObjectName("tableHeader");
    auto *row = new QHBoxLayout(header);
    row->setContentsMargins(14, 10, 14, 10);
    row->setSpacing(8);
    for (const auto &text : headers) {
        auto *label = new QLabel(text, header);
        label->setObjectName("tableHeaderText");
        row->addWidget(label, 1);
    }
    layout->addWidget(header);

    auto *empty = new QWidget(result);
    empty->setObjectName("emptyState");
    auto *emptyLayout = new QVBoxLayout(empty);
    emptyLayout->setSpacing(8);
    emptyLayout->addStretch();
    auto *icon = new QLabel(empty);
    icon->setObjectName("emptyIcon");
    icon->setPixmap(uiicons::icon(uiicons::Symbol::Empty).pixmap(32, 32));
    icon->setAlignment(Qt::AlignCenter);
    auto *titleLabel = new QLabel("暂无数据", empty);
    titleLabel->setObjectName("emptyTitle");
    titleLabel->setAlignment(Qt::AlignCenter);
    auto *note = new QLabel("相关业务将在后续阶段接入", empty);
    note->setObjectName("emptyNote");
    note->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(icon);
    emptyLayout->addWidget(titleLabel);
    emptyLayout->addWidget(note);
    emptyLayout->addStretch();
    empty->setMinimumHeight(144);
    layout->addWidget(empty, 1);
    return result;
}

QScrollArea *scrollPage(QWidget *content, QWidget *parent)
{
    auto *scroll = new QScrollArea(parent);
    scroll->setObjectName("pageScrollArea");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(content);
    return scroll;
}

QVBoxLayout *modalLayout(QDialog *dialog, const QString &title, const QString &subtitle)
{
    dialog->setObjectName(QStringLiteral("adminModal"));
    dialog->setMinimumWidth(460);
    auto *root = new QVBoxLayout(dialog);
    root->setContentsMargins(28, 24, 28, 24);
    root->setSpacing(18);

    auto *header = new QHBoxLayout;
    auto *heading = new QVBoxLayout;
    heading->setSpacing(4);
    auto *titleLabel = new QLabel(title, dialog);
    titleLabel->setProperty("role", "modalTitle");
    auto *subtitleLabel = new QLabel(subtitle, dialog);
    subtitleLabel->setProperty("role", "modalSubtitle");
    heading->addWidget(titleLabel);
    heading->addWidget(subtitleLabel);
    header->addLayout(heading);
    header->addStretch();
    auto *close = new QPushButton(QStringLiteral("×"), dialog);
    close->setObjectName(QStringLiteral("dialogCloseButton"));
    close->setFixedSize(32, 32);
    header->addWidget(close, 0, Qt::AlignTop);
    root->addLayout(header);
    QObject::connect(close, &QPushButton::clicked, dialog, &QDialog::reject);
    return root;
}

void addModalField(QVBoxLayout *form, QWidget *parent, const QString &label, QWidget *field)
{
    auto *fieldLayout = new QVBoxLayout;
    fieldLayout->setSpacing(5);
    auto *labelWidget = new QLabel(label, parent);
    labelWidget->setProperty("role", "fieldLabel");
    fieldLayout->addWidget(labelWidget);
    fieldLayout->addWidget(field);
    form->addLayout(fieldLayout);
}

QDialogButtonBox *modalButtons(QDialog *dialog, const QString &primaryText,
                               const QString &secondaryText)
{
    const auto standard = secondaryText.isEmpty()
        ? QDialogButtonBox::Close : QDialogButtonBox::Ok | QDialogButtonBox::Cancel;
    auto *buttons = new QDialogButtonBox(standard, dialog);
    auto *primary = buttons->button(secondaryText.isEmpty()
        ? QDialogButtonBox::Close : QDialogButtonBox::Ok);
    primary->setText(primaryText);
    primary->setProperty("buttonRole", secondaryText.isEmpty() ? "secondary" : "primary");
    if (!secondaryText.isEmpty()) {
        auto *secondary = buttons->button(QDialogButtonBox::Cancel);
        secondary->setText(secondaryText);
        secondary->setProperty("buttonRole", "secondary");
    }
    return buttons;
}

}  // namespace ncs::dashboard
