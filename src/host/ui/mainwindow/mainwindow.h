#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointer>
#include <QStringList>
#include "panes/pane.h"

class Editor;
class Assembler;
class Splitter;
class QCloseEvent;
class QNetworkAccessManager;
class QNetworkReply;

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
    void toggleCommentOutSelection();
    bool hasEditTarget() const;
    void openCommandLinePath(const QString &path);
    void offerSessionRecovery();
    void focusEditor();
    QString currentRomFilePath() const;
    QString currentProjectDirectory() const;

signals:
    void luaFunctionRequested(const QString &functionName, const QString &arguments);

public slots:
    void openFileDialog();
    void openRomFile();
    void openProjectFolder();
    void openProject();
    void openSettings();
    void assembleAndUpdateChecksum();
    void assembleUpdateChecksumAndOpenProject64();
    void checkForUpdates();
    void backupRom();
    void restoreRomVersion();
    void showAboutDialog();
    void openAppDataFolder();
    void saveCurrentFile();
    void saveCurrentFileAs();
    void appendLuaOutput(const QString &text);
    void appendLuaError(const QString &source, const QString &message);

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    Ui::MainWindow *ui;
    Splitter *splitter = nullptr;
    Editor *activeEditor = nullptr;
    QPointer<QWidget> lastEditTarget;
    QStringList luaOutputHistory;
    QString projectRootPath;
    QString projectRomFilePath;
    QString projectBackupDirectoryPath;
    QString standaloneRomFilePath;
    QJsonObject generalSettings;
    QJsonArray macros;
    QNetworkAccessManager *updateNetworkManager = nullptr;
    QNetworkReply *updateCheckReply = nullptr;
    bool updateCheckWasManual = false;
    bool shuttingDown = false;

    Editor *findActiveEditor() const;
    bool configureAssemblerRom(Assembler *assembler);
    void applyGeneralAppearance();
    void applyEditorPreferences();
    void applyAssemblerPreferences();
    void applyKeyboardShortcuts();
    void applyMacros();
    void startUpdateCheck(bool manual);
    void updateEditActionStates();
    QStringList recentProjectPaths() const;
    void rememberRecentProject(const QString &folderPath);
    void openProjectFolderPath(const QString &folderPath);
    void registerEditor(Editor *editor);
    void updateSnippetViews(const QString &instruction);
    void createPaneContent(Pane *pane, PaneKind kind);
    bool saveSession();
};
#endif // MAINWINDOW_H
