#include "menubar.h"
#include "ui_mainwindow.h"

#include <QAction>
#include <QList>
#include <QMenu>
#include <QKeySequence>
#include <QObject>


MenuBar::MenuBar(Ui::MainWindow *ui, MainWindow *window)
    : ui(ui), window(window) {}

// FUCKING MESS LMFAOOO
// HOW ELSE AM I SUPPOSED TO DO TS
void MenuBar::initialize() {
    MainWindow *mainWindow = window;
    QMenu* fileMenu = new QMenu(QObject::tr("File"), ui->menubar);
    fileMenu->setObjectName(QStringLiteral("fileMenu"));

    QAction* openFile = new QAction(QObject::tr("Open File"), fileMenu);
    openFile->setObjectName(QStringLiteral("openFileAction"));
    openFile->setShortcut(QKeySequence::fromString("Ctrl+O"));
    openFile->setIcon(QIcon::fromTheme("document-open"));
    fileMenu->addAction(openFile);
    QObject::connect(openFile, &QAction::triggered, window, &MainWindow::openFileDialog);

    QAction *openRom = new QAction(QObject::tr("Open ROM..."), fileMenu);
    openRom->setObjectName(QStringLiteral("openRomAction"));
    fileMenu->addAction(openRom);
    QObject::connect(openRom, &QAction::triggered, window, &MainWindow::openRomFile);

    QAction* openFolder = new QAction(QObject::tr("Open Folder..."), fileMenu);
    openFolder->setObjectName(QStringLiteral("openFolderAction"));
    openFolder->setShortcutContext(Qt::WindowShortcut);
    fileMenu->addAction(openFolder);
    QObject::connect(openFolder, &QAction::triggered, window, &MainWindow::openProjectFolder);

    QAction* openProject = new QAction(QObject::tr("Open Project..."), fileMenu);
    openProject->setObjectName(QStringLiteral("openProjectAction"));
    openProject->setShortcutContext(Qt::WindowShortcut);
    fileMenu->addAction(openProject);
    QObject::connect(openProject, &QAction::triggered, window, &MainWindow::openProject);

    QAction* saveFile = new QAction(QObject::tr("Save File"), fileMenu);
    saveFile->setObjectName(QStringLiteral("saveFileAction"));
    saveFile->setShortcut(QKeySequence::fromString("Ctrl+S"));
    saveFile->setIcon(QIcon::fromTheme("document-save"));
    fileMenu->addAction(saveFile);
    QObject::connect(saveFile, &QAction::triggered, window, &MainWindow::saveCurrentFile);

    QAction* saveFileAs = new QAction(QObject::tr("Save File As..."), fileMenu);
    saveFileAs->setObjectName(QStringLiteral("saveFileAsAction"));
    saveFileAs->setShortcut(QKeySequence::fromString("Ctrl+Shift+S"));
    fileMenu->addAction(saveFileAs);
    QObject::connect(saveFileAs, &QAction::triggered, window, &MainWindow::saveCurrentFileAs);

    ui->menubar->addMenu(fileMenu);

    QMenu* editMenu = new QMenu(QObject::tr("Edit"), ui->menubar);
    editMenu->setObjectName(QStringLiteral("editMenu"));
    QList<QAction *> editActions;
    const auto addEditAction = [&](const QString &text,
                                   const QKeySequence &shortcut,
                                   MainWindow::EditAction editAction,
                                   const QString &objectName) {
        QAction *action = editMenu->addAction(text);
        action->setObjectName(objectName);
        action->setShortcut(shortcut);
        editActions.append(action);
        QObject::connect(action, &QAction::triggered, mainWindow, [mainWindow, editAction] {
            mainWindow->performEditAction(editAction);
        });
    };

    addEditAction(QObject::tr("Undo"), QKeySequence::Undo, MainWindow::EditAction::Undo,
                  QStringLiteral("undoAction"));
    addEditAction(QObject::tr("Redo"), QKeySequence::Redo, MainWindow::EditAction::Redo,
                  QStringLiteral("redoAction"));
    editMenu->addSeparator();
    addEditAction(QObject::tr("Cut"), QKeySequence::Cut, MainWindow::EditAction::Cut,
                  QStringLiteral("cutAction"));
    addEditAction(QObject::tr("Copy"), QKeySequence::Copy, MainWindow::EditAction::Copy,
                  QStringLiteral("copyAction"));
    addEditAction(QObject::tr("Paste"), QKeySequence::Paste, MainWindow::EditAction::Paste,
                  QStringLiteral("pasteAction"));
    editMenu->addSeparator();
    addEditAction(QObject::tr("Select All"), QKeySequence::SelectAll,
                  MainWindow::EditAction::SelectAll, QStringLiteral("selectAllAction"));
    editMenu->addSeparator();
    QAction *commentOutAction = editMenu->addAction(QObject::tr("Comment Out"));
    commentOutAction->setObjectName(QStringLiteral("commentOutAction"));
    editActions.append(commentOutAction);
    QObject::connect(commentOutAction, &QAction::triggered,
                     mainWindow, &MainWindow::toggleCommentOutSelection);

    QObject::connect(editMenu, &QMenu::aboutToShow, mainWindow, [mainWindow, editActions] {
        const bool enabled = mainWindow->hasEditTarget();
        for (QAction *action : editActions)
            action->setEnabled(enabled);
    });
    ui->menubar->addMenu(editMenu);

    QMenu* assembleMenu = new QMenu(QObject::tr("Assemble"), ui->menubar);
    QAction *assembleAction = assembleMenu->addAction(
        QObject::tr("Assemble and Update Checksum"));
    assembleAction->setObjectName(QStringLiteral("assembleAction"));
    assembleAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_A));
    assembleAction->setShortcutContext(Qt::WindowShortcut);
    QObject::connect(assembleAction, &QAction::triggered,
                     window, &MainWindow::assembleAndUpdateChecksum);
    QAction *assembleAndOpenProject64Action = assembleMenu->addAction(
        QObject::tr("Assemble, Update Checksum, Focus PJ64"));
    assembleAndOpenProject64Action->setObjectName(
        QStringLiteral("assembleUpdateChecksumOpenProject64Action"));
    assembleAndOpenProject64Action->setShortcut(
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_P));
    assembleAndOpenProject64Action->setShortcutContext(Qt::WindowShortcut);
    QObject::connect(assembleAndOpenProject64Action, &QAction::triggered,
                     window, &MainWindow::assembleUpdateChecksumAndOpenProject64);
    ui->menubar->addMenu(assembleMenu);

    QMenu* backupMenu = new QMenu(QObject::tr("Backup"), ui->menubar);
    QAction *backupRomAction = backupMenu->addAction(QObject::tr("Backup ROM"));
    backupRomAction->setObjectName(QStringLiteral("backupRomAction"));
    backupRomAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Alt+B")));
    backupRomAction->setShortcutContext(Qt::WindowShortcut);
    QObject::connect(backupRomAction, &QAction::triggered,
                     window, &MainWindow::backupRom);

    QAction *restoreRomAction = backupMenu->addAction(QObject::tr("Restore Version..."));
    restoreRomAction->setObjectName(QStringLiteral("restoreRomVersionAction"));
    restoreRomAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Alt+R")));
    restoreRomAction->setShortcutContext(Qt::WindowShortcut);
    QObject::connect(restoreRomAction, &QAction::triggered,
                     window, &MainWindow::restoreRomVersion);
    ui->menubar->addMenu(backupMenu);

    QMenu* settingsMenu = new QMenu(QObject::tr("Settings"), ui->menubar);
    settingsMenu->setObjectName(QStringLiteral("settingsMenu"));
    QAction *openSettings = settingsMenu->addAction(QObject::tr("Settings..."));
    openSettings->setObjectName(QStringLiteral("openSettingsAction"));
    openSettings->setShortcutContext(Qt::WindowShortcut);
    QObject::connect(openSettings, &QAction::triggered,
                     window, &MainWindow::openSettings);
    ui->menubar->addMenu(settingsMenu);

    QMenu *helpMenu = new QMenu(QObject::tr("Help"), ui->menubar);
    QAction *checkForUpdates = helpMenu->addAction(QObject::tr("Check for Updates..."));
    checkForUpdates->setObjectName(QStringLiteral("checkForUpdatesAction"));
    QObject::connect(checkForUpdates, &QAction::triggered,
                     window, &MainWindow::checkForUpdates);
    helpMenu->addSeparator();
    QAction *aboutAction = helpMenu->addAction(QObject::tr("About"));
    QObject::connect(aboutAction, &QAction::triggered,
                     window, &MainWindow::showAboutDialog);
    QAction *openAppDataAction = helpMenu->addAction(QObject::tr("Open App Data"));
    QObject::connect(openAppDataAction, &QAction::triggered,
                     window, &MainWindow::openAppDataFolder);
    ui->menubar->addMenu(helpMenu);
}
