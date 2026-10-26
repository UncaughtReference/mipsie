#include "pane.h"

#include <QAction>
#include <QApplication>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QPaintEvent>
#include <QPainter>
#include <QPen>
#include <QVBoxLayout>

Pane::Pane(QWidget *parent)
    : QWidget(parent),
      rootLayout(new QVBoxLayout(this))
{
    rootLayout->setContentsMargins(1, 1, 1, 1);
    rootLayout->setSpacing(0);

    connect(qApp, &QApplication::focusChanged, this,
            [this](QWidget *, QWidget *nowFocused) {
        const bool focused = nowFocused && (nowFocused == this || isAncestorOf(nowFocused));
        if (paneFocused == focused)
            return;

        paneFocused = focused;
        update();
    });

    auto *paneMenuBar = new QMenuBar(this);
    auto *splitMenu = paneMenuBar->addMenu(tr("Split"));
    QAction *horizontalAction = splitMenu->addAction(tr("Side by Side"));
    QAction *verticalAction = splitMenu->addAction(tr("Top and Bottom"));
    connect(horizontalAction, &QAction::triggered, this, [this] {
        emit splitRequested(this, Qt::Horizontal);
    });
    connect(verticalAction, &QAction::triggered, this, [this] {
        emit splitRequested(this, Qt::Vertical);
    });

    auto *contentMenu = paneMenuBar->addMenu(tr("Pane"));
    const auto addPaneChoice = [this, contentMenu](const QString &title, PaneKind kind) {
        QAction *action = contentMenu->addAction(title);
        action->setCheckable(true);
        action->setData(static_cast<int>(kind));
        paneChoiceActions.append(action);
        connect(action, &QAction::triggered, this, [this, kind] {
            emit contentRequested(this, kind);
            updatePaneChoiceChecks();
        });
    };
    addPaneChoice(tr("Editor"), PaneKind::Editor);
    addPaneChoice(tr("File Tree"), PaneKind::FileTree);
    addPaneChoice(tr("Function View"), PaneKind::FunctionView);
    addPaneChoice(tr("RAM Address View"), PaneKind::RamAddressView);
    addPaneChoice(tr("Struct View"), PaneKind::StructView);
    addPaneChoice(tr("Mario Actions View"), PaneKind::MarioActionsView);
    addPaneChoice(tr("EEPROM View"), PaneKind::EepromView);
    addPaneChoice(tr("Assembler"), PaneKind::Assembler);
    addPaneChoice(tr("Snippets"), PaneKind::Snippets);
    addPaneChoice(tr("Lua Console"), PaneKind::LuaConsole);
    contentMenu->addSeparator();
    QAction *deleteAction = contentMenu->addAction(tr("Delete Pane"));
    connect(deleteAction, &QAction::triggered, this, [this] {
        emit deleteRequested(this);
    });

    auto *placeholder = new QLabel(tr("Empty pane - choose a pane type above!"), this);
    placeholder->setAlignment(Qt::AlignCenter);
    rootLayout->addWidget(paneMenuBar);
    rootLayout->addWidget(placeholder, 1);
    contentWidget = placeholder;
}

void Pane::setContent(QWidget *widget, PaneKind kind)
{
    if (!widget || widget == contentWidget)
        return;

    rootLayout->removeWidget(contentWidget);
    delete contentWidget;

    contentWidget = widget;
    paneKind = kind;
    updatePaneChoiceChecks();
    widget->setParent(this);
    rootLayout->insertWidget(1, widget, 1);
}

void Pane::updatePaneChoiceChecks()
{
    for (QAction *action : paneChoiceActions)
        action->setChecked(action->data().toInt() == static_cast<int>(paneKind));
}

PaneKind Pane::kind() const
{
    return paneKind;
}

void Pane::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);

    QPainter painter(this);
    painter.setPen(QPen(paneFocused ? QColor("#ff4da6") : QColor("#454545"), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(rect().adjusted(0, 0, -1, -1));
}
