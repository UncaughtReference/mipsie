#include "pane.h"
#include "editor/editor.h"

#include <QAction>
#include <QApplication>
#include <QColor>
#include <QLabel>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QPaintEvent>
#include <QPainter>
#include <QPen>
#include <QTextEdit>
#include <QTimer>
#include <QTreeView>
#include <QVBoxLayout>

Pane::Pane(QWidget *parent)
    : QWidget(parent),
      rootLayout(new QVBoxLayout(this))
{
    setFocusPolicy(Qt::StrongFocus);
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
    addPaneChoice(tr("Command Prompt"), PaneKind::CmdConsole);
    contentMenu->addSeparator();
    QAction *deleteAction = contentMenu->addAction(tr("Delete Pane"));
    connect(deleteAction, &QAction::triggered, this, [this] {
        emit deleteRequested(this);
    });

    shortcutLabelAction = paneMenuBar->addAction(QString());
    shortcutLabelAction->setObjectName(QStringLiteral("paneShortcutLabelAction"));
    shortcutLabelAction->setEnabled(false);

    focusShortcutAction = new QAction(this);
    focusShortcutAction->setObjectName(QStringLiteral("focusPaneAction"));
    focusShortcutAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(focusShortcutAction, &QAction::triggered, this, [this] {
        if (QWidget *paneFocusInput = contentWidget->findChild<QWidget *>(
                QStringLiteral("paneFocusInput"))) {
            paneFocusInput->setFocus(Qt::ShortcutFocusReason);
            return;
        }
        if (auto *treeView = contentWidget->findChild<QTreeView *>()) {
            treeView->setFocus(Qt::ShortcutFocusReason);
            return;
        }
        if (auto *editor = qobject_cast<Editor *>(contentWidget)) {
            if (QTextEdit *textEditor = editor->currentEditor()) {
                textEditor->setFocus(Qt::ShortcutFocusReason);
                return;
            }
        }
        if (auto *textEditor = contentWidget->findChild<QTextEdit *>()) {
            textEditor->setFocus(Qt::ShortcutFocusReason);
            return;
        }
        QWidget *focusTarget = contentWidget;
        if (focusTarget && focusTarget->focusPolicy() == Qt::NoFocus) {
            const auto children = focusTarget->findChildren<QWidget *>();
            for (QWidget *child : children) {
                if (child->focusPolicy() != Qt::NoFocus) {
                    focusTarget = child;
                    break;
                }
            }
        }
        if (!focusTarget || focusTarget->focusPolicy() == Qt::NoFocus)
            focusTarget = this;
        focusTarget->setFocus(Qt::ShortcutFocusReason);
    });

    auto *placeholder = new QLabel(tr("Empty pane - choose a pane type above!"), this);
    placeholder->setAlignment(Qt::AlignCenter);
    rootLayout->addWidget(paneMenuBar);
    rootLayout->addWidget(placeholder, 1);
    contentWidget = placeholder;
}

void Pane::setPaneShortcut(const QKeySequence &shortcut)
{
    focusShortcutAction->setShortcut(shortcut);
    shortcutLabelAction->setText(shortcut.toString(QKeySequence::NativeText));
    shortcutLabelAction->setVisible(!shortcut.isEmpty());
    QTimer::singleShot(0, this, [this] {
        QWidget *windowWidget = window();
        if (!windowWidget)
            return;
        if (shortcutHost && shortcutHost != windowWidget)
            shortcutHost->removeAction(focusShortcutAction);
        windowWidget->addAction(focusShortcutAction);
        shortcutHost = windowWidget;
    });
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
    QColor focusColor = qApp->property("mipsieAccentColor").value<QColor>();
    if (!focusColor.isValid())
        focusColor = QColor(QStringLiteral("#ff4da6"));
    painter.setPen(QPen(paneFocused ? focusColor : QColor("#454545"), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(rect().adjusted(0, 0, -1, -1));
}
