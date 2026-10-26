#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPointer>
#include <QStringList>
#include "panes/pane.h"

class Editor;
class Splitter;
class QCloseEvent;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    enum class EditAction
    {
        Undo,
        Redo,
        Cut,
        Copy,
        Paste,
        SelectAll
    };

    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void showMainWindow(
        int width,
        int height,
        bool maximized
        );

    void performEditAction(EditAction action);
    bool hasEditTarget() const;

public slots:
    void openFileDialog();
    void openProjectFolder();
    void assembleAndUpdateChecksum();
    void backupRom();
    void restoreRomVersion();
    void saveCurrentFile();
    void saveCurrentFileAs();
    void appendLuaOutput(const QString &text);
    void appendLuaError(const QString &source, const QString &message);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    Ui::MainWindow *ui;
    Splitter *splitter = nullptr;
    Editor *activeEditor = nullptr;
    QPointer<QWidget> lastEditTarget;
    QStringList luaOutputHistory;
    QString projectRootPath;
    QString projectRomFilePath;
    bool shuttingDown = false;

    Editor *findActiveEditor() const;
    void registerEditor(Editor *editor);
    void updateSnippetViews(const QString &instruction);
    void createPaneContent(Pane *pane, PaneKind kind);
};
#endif // MAINWINDOW_H
