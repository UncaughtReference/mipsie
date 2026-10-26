#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "menubar.h"
#include "splitter.h"
#include "panes/pane.h"
#include "panes/luaconsole/luaconsole.h"
#include "panes/editor.h"
#include "panes/filetree.h"
#include "panes/functionview/functionview.h"
#include "panes/ramaddressview/ramaddressview.h"
#include "panes/structview/structview.h"
#include "panes/marioactionsview/marioactionsview.h"
#include "panes/eepromview/eepromview.h"
#include "panes/assembler/assembler.h"
#include "panes/snippets/snippets.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTimer>
#include <QVBoxLayout>

namespace
{
constexpr int BackupRomPathRole = Qt::UserRole + 1;
constexpr int BackupConfigPathRole = Qt::UserRole + 2;
const QString backupTimestampFormat = QStringLiteral("yyyy-MM-dd_HH-mm-ss-zzz");

QString configFilePathForRom(const QString &romFilePath)
{
    const QFileInfo romInfo(romFilePath);
    return romInfo.dir().filePath(romInfo.completeBaseName() + QStringLiteral(".config"));
}

QString backupRomFileName(const QString &stamp, const QString &romSuffix)
{
    return romSuffix.isEmpty() ? stamp : stamp + QLatin1Char('.') + romSuffix;
}

QString uniqueBackupStem(const QString &directoryPath, const QString &romSuffix)
{
    const QString timestamp = QDateTime::currentDateTime().toString(backupTimestampFormat);
    QString candidate = timestamp;
    int duplicateIndex = 1;
    const QDir directory(directoryPath);
    while (QFileInfo::exists(directory.filePath(backupRomFileName(candidate, romSuffix)))
           || QFileInfo::exists(directory.filePath(candidate + QStringLiteral(".config")))) {
        candidate = timestamp + QLatin1Char('_') + QString::number(duplicateIndex++);
    }
    return candidate;
}

bool copyFileAtomically(const QString &sourcePath,
                        const QString &destinationPath,
                        QString *errorMessage)
{
    QFile source(sourcePath);
    if (!source.open(QIODevice::ReadOnly)) {
        if (errorMessage)
            *errorMessage = source.errorString();
        return false;
    }

    QSaveFile destination(destinationPath);
    if (!destination.open(QIODevice::WriteOnly)) {
        if (errorMessage)
            *errorMessage = destination.errorString();
        return false;
    }

    while (!source.atEnd()) {
        const QByteArray chunk = source.read(1024 * 1024);
        if (chunk.isEmpty()) {
            if (errorMessage)
                *errorMessage = source.errorString();
            destination.cancelWriting();
            return false;
        }
        if (destination.write(chunk) != chunk.size()) {
            if (errorMessage)
                *errorMessage = destination.errorString();
            destination.cancelWriting();
            return false;
        }
    }

    if (!destination.commit()) {
        if (errorMessage)
            *errorMessage = destination.errorString();
        return false;
    }
    return true;
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    this->setWindowTitle("mipsie");

    MenuBar menuBar(ui, this);
    menuBar.initialize();

    splitter = new Splitter(Qt::Horizontal, this);

    auto *fileTreePane = new Pane;
    auto *fileTree = new FileTree(fileTreePane);
    fileTree->initialize(projectRootPath);
    fileTreePane->setContent(fileTree, PaneKind::FileTree);
    splitter->addPane(fileTreePane);

    auto *workspaceSplitter = new Splitter(Qt::Horizontal, splitter);
    auto *editorColumn = new Splitter(Qt::Vertical, workspaceSplitter);
    auto *snippetsColumn = new Splitter(Qt::Vertical, workspaceSplitter);
    splitter->addWidget(workspaceSplitter);
    workspaceSplitter->addWidget(editorColumn);
    workspaceSplitter->addWidget(snippetsColumn);

    auto *editorPane = new Pane;
    auto *editor = new Editor(editorPane);
    editor->initialize();
    editorPane->setContent(editor, PaneKind::Editor);
    editorColumn->addWidget(editorPane);
    splitter->registerPane(editorPane);

    auto *assemblerPane = new Pane;
    auto *assembler = new Assembler(assemblerPane);
    assembler->setRomFilePath(projectRomFilePath);
    assemblerPane->setContent(assembler, PaneKind::Assembler);
    editorColumn->addWidget(assemblerPane);
    splitter->registerPane(assemblerPane);

    auto *snippetsPane = new Pane;
    auto *snippets = new Snippets(snippetsPane);
    snippetsPane->setContent(snippets, PaneKind::Snippets);
    snippetsColumn->addWidget(snippetsPane);
    splitter->registerPane(snippetsPane);

    auto *functionViewPane = new Pane;
    functionViewPane->setContent(new FunctionView(functionViewPane), PaneKind::FunctionView);
    snippetsColumn->addWidget(functionViewPane);
    splitter->registerPane(functionViewPane);

    splitter->setInitialSizes({250, 1000});
    workspaceSplitter->setInitialSizes({650, 350});
    editorColumn->setInitialSizes({700, 300});
    snippetsColumn->setInitialSizes({550, 450});

    setCentralWidget(splitter);

    activeEditor = editor;
    registerEditor(editor);
    snippets->showInstruction(editor->currentInstruction());
    connect(splitter, &Splitter::contentRequested,
            this, &MainWindow::createPaneContent);
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget *, QWidget *now) {
        if (shuttingDown)
            return;

        if (now) {
            QWidget *editTarget = now;
            while (editTarget
                   && !qobject_cast<QTextEdit *>(editTarget)
                   && !qobject_cast<QLineEdit *>(editTarget)) {
                editTarget = editTarget->parentWidget();
            }

            if (editTarget)
                lastEditTarget = editTarget;
            else if (!qobject_cast<QMenu *>(now))
                lastEditTarget.clear();
        }

        if (!now || !splitter)
            return;
        const QList<Editor *> editors = splitter->findChildren<Editor *>();
        for (Editor *candidate : editors) {
            if (candidate == now || candidate->isAncestorOf(now)) {
                if (activeEditor != candidate) {
                    activeEditor = candidate;
                    updateSnippetViews(candidate->currentInstruction());
                }
                return;
            }
        }
    });
    connect(fileTree, &FileTree::fileDoubleClicked, this, [this](const QString &path) {
        if (Editor *editor = findActiveEditor())
            editor->openFile(path);
    });
}

bool MainWindow::hasEditTarget() const
{
    return !lastEditTarget.isNull();
}

void MainWindow::performEditAction(EditAction action)
{
    QWidget *target = lastEditTarget.data();
    if (!target)
        return;

    if (auto *textEdit = qobject_cast<QTextEdit *>(target)) {
        switch (action) {
        case EditAction::Undo: textEdit->undo(); break;
        case EditAction::Redo: textEdit->redo(); break;
        case EditAction::Cut: textEdit->cut(); break;
        case EditAction::Copy: textEdit->copy(); break;
        case EditAction::Paste: textEdit->paste(); break;
        case EditAction::SelectAll: textEdit->selectAll(); break;
        }
        return;
    }

    if (auto *lineEdit = qobject_cast<QLineEdit *>(target)) {
        switch (action) {
        case EditAction::Undo: lineEdit->undo(); break;
        case EditAction::Redo: lineEdit->redo(); break;
        case EditAction::Cut: lineEdit->cut(); break;
        case EditAction::Copy: lineEdit->copy(); break;
        case EditAction::Paste: lineEdit->paste(); break;
        case EditAction::SelectAll: lineEdit->selectAll(); break;
        }
    }
}

Editor *MainWindow::findActiveEditor() const
{
    if (shuttingDown)
        return nullptr;
    if (activeEditor && splitter && splitter->isAncestorOf(activeEditor))
        return activeEditor;
    return splitter ? splitter->findChild<Editor *>() : nullptr;
}

void MainWindow::registerEditor(Editor *editor)
{
    if (!editor)
        return;

    connect(editor, &Editor::currentInstructionChanged, this,
            [this, editor](const QString &instruction) {
        if (!shuttingDown && activeEditor == editor)
            updateSnippetViews(instruction);
    });
    connect(editor, &QObject::destroyed, this, [this, editor] {
        if (shuttingDown)
            return;
        if (activeEditor != editor)
            return;
        activeEditor = nullptr;
        updateSnippetViews(QString());
        QTimer::singleShot(0, this, [this] {
            if (shuttingDown)
                return;
            activeEditor = splitter ? splitter->findChild<Editor *>() : nullptr;
            updateSnippetViews(activeEditor ? activeEditor->currentInstruction() : QString());
        });
    });
}

void MainWindow::updateSnippetViews(const QString &instruction)
{
    if (shuttingDown || !splitter)
        return;
    const auto views = splitter->findChildren<Snippets *>();
    for (Snippets *view : views)
        view->showInstruction(instruction);
}

void MainWindow::createPaneContent(Pane *pane, PaneKind kind)
{
    if (shuttingDown || !pane)
        return;

    if (pane->kind() == kind)
        return;
    if (kind == PaneKind::Editor && splitter) {
        if (Editor *existingEditor = splitter->findChild<Editor *>()) {
            QMessageBox::information(
                this,
                tr("Editor Already Open"),
                tr("Only one Editor pane can be open at a time. The existing Editor has been focused."));
            activeEditor = existingEditor;
            if (QTextEdit *textEditor = existingEditor->currentEditor())
                textEditor->setFocus();
            return;
        }
    }
    if (pane->kind() == PaneKind::Editor) {
        const auto choice = QMessageBox::warning(
            this,
            tr("Replace Pane Content?"),
            tr("Replacing this pane will close its current content. Unsaved editor changes may be lost."),
            QMessageBox::Yes | QMessageBox::Cancel,
            QMessageBox::Cancel);
        if (choice != QMessageBox::Yes)
            return;
    }

    switch (kind) {
    case PaneKind::Editor: {
        auto *editor = new Editor(pane);
        editor->initialize();
        pane->setContent(editor, PaneKind::Editor);
        activeEditor = editor;
        registerEditor(editor);
        break;
    }
    case PaneKind::FileTree: {
        auto *fileTree = new FileTree(pane);
        fileTree->initialize(projectRootPath);
        connect(fileTree, &FileTree::fileDoubleClicked, this, [this](const QString &path) {
            if (Editor *editor = findActiveEditor())
                editor->openFile(path);
        });
        pane->setContent(fileTree, PaneKind::FileTree);
        break;
    }
    case PaneKind::FunctionView: {
        pane->setContent(new FunctionView(pane), PaneKind::FunctionView);
        break;
    }
    case PaneKind::RamAddressView: {
        pane->setContent(new RamAddressView(pane), PaneKind::RamAddressView);
        break;
    }
    case PaneKind::StructView: {
        pane->setContent(new StructView(pane), PaneKind::StructView);
        break;
    }
    case PaneKind::MarioActionsView: {
        pane->setContent(new MarioActionsView(pane), PaneKind::MarioActionsView);
        break;
    }
    case PaneKind::EepromView: {
        pane->setContent(new EepromView(pane), PaneKind::EepromView);
        break;
    }
    case PaneKind::Assembler: {
        auto *assembler = new Assembler(pane);
        assembler->setRomFilePath(projectRomFilePath);
        pane->setContent(assembler, PaneKind::Assembler);
        break;
    }
    case PaneKind::Snippets: {
        auto *snippets = new Snippets(pane);
        pane->setContent(snippets, PaneKind::Snippets);
        snippets->showInstruction(activeEditor ? activeEditor->currentInstruction() : QString());
        break;
    }
    case PaneKind::LuaConsole: {
        auto *console = new LuaConsole(pane);
        pane->setContent(console, PaneKind::LuaConsole);
        for (const QString &line : luaOutputHistory)
            console->appendOutput(line);
        break;
    }
    case PaneKind::Empty:
        break;
    }
}

void MainWindow::appendLuaOutput(const QString &text)
{
    if (shuttingDown)
        return;
    luaOutputHistory.append(text);
    if (!splitter)
        return;

    const auto consoles = splitter->findChildren<LuaConsole *>();
    for (LuaConsole *console : consoles)
        console->appendOutput(text);
}

void MainWindow::appendLuaError(const QString &source, const QString &message)
{
    appendLuaOutput(QStringLiteral("[Lua Error] %1: %2").arg(source, message));
}

void MainWindow::openFileDialog()
{
    Editor *editor = findActiveEditor();
    if (!editor)
        return;

    const QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("Open Assembly File"),
        QString(),
        tr("Assembly files (*.asm *.s *.inc);;All files (*)"));
    if (!filePath.isEmpty())
        editor->openFile(filePath);
}

void MainWindow::openProjectFolder()
{
    if (shuttingDown)
        return;

    const QString selectedFolder = QFileDialog::getExistingDirectory(
        this,
        tr("Open Project Folder"),
        projectRootPath.isEmpty() ? QDir::homePath() : projectRootPath);
    if (selectedFolder.isEmpty())
        return;

    const QString folderPath = QDir(selectedFolder).absolutePath();
    const QString configPath = QDir(folderPath).filePath(QStringLiteral("mipsie.json"));
    QString romFilePath;

    if (QFileInfo::exists(configPath)) {
        if (!QFileInfo(configPath).isFile()) {
            QMessageBox::warning(this, tr("Invalid Project"),
                                 tr("mipsie.json exists but is not a file."));
            return;
        }

        QFile configFile(configPath);
        if (!configFile.open(QIODevice::ReadOnly)) {
            QMessageBox::warning(this, tr("Could Not Read Project"),
                                 tr("Could not read %1: %2")
                                     .arg(configPath, configFile.errorString()));
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(configFile.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            QMessageBox::warning(this, tr("Invalid Project"),
                                 tr("Could not parse %1: %2")
                                     .arg(configPath, parseError.errorString()));
            return;
        }

        const QJsonValue romPathValue = document.object().value(QStringLiteral("romFilePath"));
        if (!romPathValue.isString() || romPathValue.toString().trimmed().isEmpty()) {
            QMessageBox::warning(this, tr("Invalid Project"),
                                 tr("mipsie.json must contain a non-empty romFilePath."));
            return;
        }

        const QString configuredPath = romPathValue.toString().trimmed();
        const QString resolvedPath = QDir::isAbsolutePath(configuredPath)
            ? configuredPath
            : QDir(folderPath).absoluteFilePath(configuredPath);
        romFilePath = QDir::toNativeSeparators(QFileInfo(resolvedPath).absoluteFilePath());
    } else {
        const QMessageBox::StandardButton answer = QMessageBox::question(
            this,
            tr("Create Project Configuration?"),
            tr("No mipsie.json was found in this folder. Would you like to create one?"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::Yes);

        if (answer == QMessageBox::Yes) {
            QDialog setupDialog(this);
            setupDialog.setWindowTitle(tr("Create Mipsie Project"));
            setupDialog.setModal(true);
            setupDialog.setMinimumWidth(520);

            auto *dialogLayout = new QVBoxLayout(&setupDialog);
            auto *description = new QLabel(
                tr("Project folder: %1\nChoose the ROM file this project will assemble into.")
                    .arg(QDir::toNativeSeparators(folderPath)),
                &setupDialog);
            dialogLayout->addWidget(description);

            auto *formLayout = new QFormLayout;
            auto *romPathField = new QLineEdit(&setupDialog);
            romPathField->setPlaceholderText(tr("Select a .z64, .n64, or .v64 file"));
            auto *romPathRow = new QWidget(&setupDialog);
            auto *romPathLayout = new QHBoxLayout(romPathRow);
            romPathLayout->setContentsMargins(0, 0, 0, 0);
            auto *browseButton = new QPushButton(tr("Browse..."), romPathRow);
            romPathLayout->addWidget(romPathField, 1);
            romPathLayout->addWidget(browseButton);
            formLayout->addRow(tr("ROM file:"), romPathRow);
            dialogLayout->addLayout(formLayout);

            auto *dialogButtons = new QDialogButtonBox(
                QDialogButtonBox::Save | QDialogButtonBox::Cancel,
                &setupDialog);
            dialogButtons->button(QDialogButtonBox::Save)->setText(tr("Create Project"));
            dialogLayout->addWidget(dialogButtons);

            connect(browseButton, &QPushButton::clicked, &setupDialog, [&] {
                const QString selectedRomPath = QFileDialog::getOpenFileName(
                    &setupDialog,
                    tr("Select ROM File"),
                    folderPath,
                    tr("Nintendo 64 ROMs (*.z64 *.n64 *.v64);;All files (*)"));
                if (!selectedRomPath.isEmpty())
                    romPathField->setText(QDir::toNativeSeparators(selectedRomPath));
            });
            connect(dialogButtons, &QDialogButtonBox::rejected,
                    &setupDialog, &QDialog::reject);
            connect(dialogButtons, &QDialogButtonBox::accepted, &setupDialog, [&] {
                const QString enteredPath = romPathField->text().trimmed();
                if (enteredPath.isEmpty()) {
                    QMessageBox::warning(&setupDialog, tr("ROM Required"),
                                         tr("Select a ROM file for this project."));
                    return;
                }

                const QString resolvedPath = QDir::isAbsolutePath(enteredPath)
                    ? enteredPath
                    : QDir(folderPath).absoluteFilePath(enteredPath);
                const QFileInfo romInfo(resolvedPath);
                if (!romInfo.isFile()) {
                    QMessageBox::warning(&setupDialog, tr("ROM Not Found"),
                                         tr("The selected ROM file does not exist."));
                    return;
                }

                romFilePath = QDir::toNativeSeparators(romInfo.absoluteFilePath());
                setupDialog.accept();
            });

            if (setupDialog.exec() != QDialog::Accepted)
                return;

            const QString storedRomPath = QDir::fromNativeSeparators(romFilePath);
            QJsonObject projectConfig;
            projectConfig.insert(QStringLiteral("romFilePath"), storedRomPath);
            const QByteArray json = QJsonDocument(projectConfig).toJson(QJsonDocument::Indented);
            QSaveFile configFile(configPath);
            if (!configFile.open(QIODevice::WriteOnly)
                || configFile.write(json) != json.size()
                || !configFile.commit()) {
                QMessageBox::warning(this, tr("Could Not Create Project"),
                                     tr("Could not write %1: %2")
                                         .arg(configPath, configFile.errorString()));
                return;
            }
        }
    }

    projectRootPath = folderPath;
    projectRomFilePath = romFilePath;
    if (!splitter)
        return;

    const auto fileTrees = splitter->findChildren<FileTree *>();
    for (FileTree *tree : fileTrees)
        tree->setRootPath(projectRootPath);

    const auto assemblers = splitter->findChildren<Assembler *>();
    for (Assembler *assembler : assemblers)
        assembler->setRomFilePath(projectRomFilePath);

    if (!projectRomFilePath.isEmpty() && !QFileInfo(projectRomFilePath).isFile()) {
        QMessageBox::warning(this, tr("ROM File Not Found"),
                             tr("The ROM configured for this project was not found: %1")
                                 .arg(projectRomFilePath));
    }
}

void MainWindow::assembleAndUpdateChecksum()
{
    if (shuttingDown || !splitter)
        return;

    Assembler *assembler = splitter->findChild<Assembler *>();
    if (!assembler) {
        QMessageBox::information(this, tr("Assembler Not Open"),
                                 tr("Create an Assembler pane before assembling."));
        return;
    }

    assembler->assemble();
}

void MainWindow::saveCurrentFile()
{
    if (Editor *editor = findActiveEditor())
        editor->saveCurrentFile();
}

void MainWindow::saveCurrentFileAs()
{
    if (Editor *editor = findActiveEditor())
        editor->saveCurrentFileAs();
}

MainWindow::~MainWindow()
{
    shuttingDown = true;
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    shuttingDown = true;
    QMainWindow::closeEvent(event);
    if (!event->isAccepted())
        shuttingDown = false;
}
