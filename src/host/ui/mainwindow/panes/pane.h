#ifndef PANE_H
#define PANE_H

#include <QWidget>
#include <QList>

class QAction;
class QVBoxLayout;
class QPaintEvent;
class QKeySequence;

enum class PaneKind
{
    Empty,
    Editor,
    FileTree,
    FunctionView,
    RamAddressView,
    StructView,
    MarioActionsView,
    EepromView,
    Assembler,
    Snippets,
    LuaConsole
};

class Pane : public QWidget
{
    Q_OBJECT

public:
    explicit Pane(QWidget *parent = nullptr);
    void setContent(QWidget *widget, PaneKind kind);
    void setPaneShortcut(const QKeySequence &shortcut);
    PaneKind kind() const;

protected:
    void paintEvent(QPaintEvent *event) override;

signals:
    void splitRequested(Pane *pane, Qt::Orientation orientation);
    void contentRequested(Pane *pane, PaneKind kind);
    void deleteRequested(Pane *pane);

private:
    QVBoxLayout *rootLayout;
    QWidget *contentWidget = nullptr;
    QList<QAction *> paneChoiceActions;
    QAction *focusShortcutAction = nullptr;
    QAction *shortcutLabelAction = nullptr;
    PaneKind paneKind = PaneKind::Empty;
    bool paneFocused = false;

    void updatePaneChoiceChecks();
};

#endif // PANE_H
