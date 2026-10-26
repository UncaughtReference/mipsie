#include "menubar.h"
#include "ui_mainwindow.h"

#include <QAction>
#include <QList>
#include <QMenu>
#include <QKeySequence>
#include <QObject>


MenuBar::MenuBar(Ui::MainWindow *ui, MainWindow *window)
    : ui(ui), window(window) {}

void MenuBar::initialize() {
    MainWindow *mainWindow = window;
    QMenu* fileMenu = new QMenu(QObject::tr("File"));

    QAction* openFile = new QAction(QObject::tr("Open File"), fileMenu);
    openFile->setShortcut(QKeySequence::fromString("Ctrl+O"));
    openFile->setIcon(QIcon::fromTheme("document-open"));
    fileMenu->addAction(openFile);
    QObject::connect(openFile, &QAction::triggered, window, &MainWindow::openFileDialog);

    QAction* openFolder = new QAction(QObject::tr("Open Folder..."), fileMenu);
    fileMenu->addAction(openFolder);
    QObject::connect(openFolder, &QAction::triggered, window, &MainWindow::openProjectFolder);

    QAction* saveFile = new QAction(QObject::tr("Save File"), fileMenu);
    saveFile->setShortcut(QKeySequence::fromString("Ctrl+S"));
    saveFile->setIcon(QIcon::fromTheme("document-save"));
    fileMenu->addAction(saveFile);
    QObject::connect(saveFile, &QAction::triggered, window, &MainWindow::saveCurrentFile);

    QAction* saveFileAs = new QAction(QObject::tr("Save File As..."), fileMenu);
    saveFileAs->setShortcut(QKeySequence::fromString("Ctrl+Shift+S"));
    fileMenu->addAction(saveFileAs);
    QObject::connect(saveFileAs, &QAction::triggered, window, &MainWindow::saveCurrentFileAs);

    ui->menubar->addMenu(fileMenu);

    QMenu* editMenu = new QMenu(QObject::tr("Edit"));
    QList<QAction *> editActions;
    const auto addEditAction = [&](const QString &text,
                                   const QKeySequence &shortcut,
                                   MainWindow::EditAction editAction) {
        QAction *action = editMenu->addAction(text);
        action->setShortcut(shortcut);
        editActions.append(action);
        QObject::connect(action, &QAction::triggered, mainWindow, [mainWindow, editAction] {
            mainWindow->performEditAction(editAction);
        });
    };

    addEditAction(QObject::tr("Undo"), QKeySequence::Undo, MainWindow::EditAction::Undo);
    addEditAction(QObject::tr("Redo"), QKeySequence::Redo, MainWindow::EditAction::Redo);
    editMenu->addSeparator();
    addEditAction(QObject::tr("Cut"), QKeySequence::Cut, MainWindow::EditAction::Cut);
    addEditAction(QObject::tr("Copy"), QKeySequence::Copy, MainWindow::EditAction::Copy);
    addEditAction(QObject::tr("Paste"), QKeySequence::Paste, MainWindow::EditAction::Paste);
    editMenu->addSeparator();
    addEditAction(QObject::tr("Select All"), QKeySequence::SelectAll,
                  MainWindow::EditAction::SelectAll);

    QObject::connect(editMenu, &QMenu::aboutToShow, mainWindow, [mainWindow, editActions] {
        const bool enabled = mainWindow->hasEditTarget();
        for (QAction *action : editActions)
            action->setEnabled(enabled);
    });
    ui->menubar->addMenu(editMenu);

    QMenu* viewMenu = new QMenu(QObject::tr("View"));
    ui->menubar->addMenu(viewMenu);

    QMenu* assembleMenu = new QMenu(QObject::tr("Assemble"));
    QAction *assembleAction = assembleMenu->addAction(
        QObject::tr("Assemble and Update Checksum"));
    assembleAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_B));
    assembleAction->setShortcutContext(Qt::WindowShortcut);
    QObject::connect(assembleAction, &QAction::triggered,
                     window, &MainWindow::assembleAndUpdateChecksum);
    ui->menubar->addMenu(assembleMenu);

    QMenu* backupMenu = new QMenu(QObject::tr("Backup"));
    QAction *backupRomAction = backupMenu->addAction(QObject::tr("Backup ROM"));
    QObject::connect(backupRomAction, &QAction::triggered,
                     window, &MainWindow::backupRom);

    QAction *restoreRomAction = backupMenu->addAction(QObject::tr("Restore Version..."));
    QObject::connect(restoreRomAction, &QAction::triggered,
                     window, &MainWindow::restoreRomVersion);
    ui->menubar->addMenu(backupMenu);

    QMenu* settingsMenu = new QMenu(QObject::tr("Settings"));
    ui->menubar->addMenu(settingsMenu);
}
