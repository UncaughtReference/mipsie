#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "menubar.h"
#include "splitter.h"
#include "panes/pane.h"
#include "panes/luaconsole/luaconsole.h"
#include "panes/editor/editor.h"
#include "panes/filetree/filetree.h"
#include "panes/functionview/functionview.h"
#include "panes/ramaddressview/ramaddressview.h"
#include "panes/structview/structview.h"
#include "panes/marioactionsview/marioactionsview.h"
#include "panes/eepromview/eepromview.h"
#include "panes/assembler/assembler.h"
#include "panes/snippets/snippets.h"
#include "panes/cmdconsole/cmdconsole.h"
#include "settingsdialog.h"

#include <QAction>
#include <QApplication>
#include <QAbstractItemView>
#include <QColor>
#include <QCloseEvent>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QFileDialog>
#include <QFormLayout>
#include <QFileInfo>
#include <QFont>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPalette>
#include <QPushButton>
#include <QSaveFile>
#include <QSettings>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QVersionNumber>
#include <QVBoxLayout>

#include <algorithm>

namespace
{
constexpr int BackupRomPathRole = Qt::UserRole + 1;
constexpr int BackupConfigPathRole = Qt::UserRole + 2;
const QString backupTimestampFormat = QStringLiteral("yyyy-MM-dd_HH-mm-ss-zzz");
constexpr int RecentProjectPathRole = Qt::UserRole + 1;

// Returns the recent projects json
QString recentProjectsFilePath()
{
    QString settingsDirectory = QStandardPaths::writableLocation(
        QStandardPaths::AppConfigLocation);
    if (settingsDirectory.isEmpty())
        settingsDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (settingsDirectory.isEmpty())
        settingsDirectory = QDir::home().filePath(QStringLiteral(".mipsie"));
    return QDir(settingsDirectory).filePath(QStringLiteral("recent_projects.json"));
}

// Returns the general settings json
QString generalSettingsFilePath()
{
    const QFileInfo recentFile(recentProjectsFilePath());
    return recentFile.dir().filePath(QStringLiteral("general_settings.json"));
}

QString macrosFilePath()
{
    return QFileInfo(recentProjectsFilePath()).dir().filePath(QStringLiteral("macros.json"));
}

QJsonArray defaultMacros()
{
    QJsonObject macro1;
    macro1.insert(QStringLiteral("shortcut"), QStringLiteral("F1"));
    macro1.insert(QStringLiteral("text"), QStringLiteral(
        "addiu sp, sp, -0x18\n"
        "sw ra, 0x14(sp)\n"
        "\n"
        "\n"
        "\n"
        "lw ra, 0x14(sp)\n"
        "jr ra\n"
        "addiu sp, sp, 0x18"));
    QJsonObject macro2;
    macro2.insert(QStringLiteral("shortcut"), QStringLiteral("F2"));
    macro2.insert(QStringLiteral("text"), QStringLiteral("lui t0, 0x8034\n"));
    QJsonObject macro3;
    macro3.insert(QStringLiteral("shortcut"), QStringLiteral("F3"));
    macro3.insert(QStringLiteral("text"), QStringLiteral("lw t6, 0x80361160\n"));
    QJsonArray macros;
    macros.append(macro1);
    macros.append(macro2);
    macros.append(macro3);
    return macros;
}

QJsonArray loadMacros()
{
    QFile file(macrosFilePath());
    if (!file.open(QIODevice::ReadOnly))
        return defaultMacros();

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray())
        return defaultMacros();

    QJsonArray macros;
    for (const QJsonValue &value : document.array()) {
        if (value.isObject())
            macros.append(value);
    }
    return macros;
}

// Set the default settings (yanderedev)
QJsonObject defaultGeneralSettings()
{
    QJsonObject keybinds;
    keybinds.insert(QStringLiteral("openFileAction"), QStringLiteral("Ctrl+O"));
    keybinds.insert(QStringLiteral("openFolderAction"), QStringLiteral("Ctrl+Shift+O"));
    keybinds.insert(QStringLiteral("openProjectAction"), QStringLiteral("Ctrl+Alt+O"));
    keybinds.insert(QStringLiteral("openSettingsAction"), QStringLiteral("Ctrl+,"));
    keybinds.insert(QStringLiteral("saveFileAction"), QStringLiteral("Ctrl+S"));
    keybinds.insert(QStringLiteral("saveFileAsAction"), QStringLiteral("Ctrl+Shift+S"));
    keybinds.insert(QStringLiteral("closeTabAction"), QStringLiteral("Ctrl+W"));
    keybinds.insert(QStringLiteral("previousTabAction"), QStringLiteral("Ctrl+PgUp"));
    keybinds.insert(QStringLiteral("nextTabAction"), QStringLiteral("Ctrl+PgDown"));
    keybinds.insert(QStringLiteral("newTabAction"), QStringLiteral("Ctrl+T"));
    keybinds.insert(QStringLiteral("backupRomAction"), QStringLiteral("Ctrl+Alt+B"));
    keybinds.insert(QStringLiteral("restoreRomVersionAction"), QStringLiteral("Ctrl+Alt+R"));
    keybinds.insert(QStringLiteral("findAction"), QStringLiteral("Ctrl+F"));
    keybinds.insert(QStringLiteral("replaceAction"), QStringLiteral("Ctrl+H"));
    keybinds.insert(QStringLiteral("searchLabelsAction"), QStringLiteral("Ctrl+Shift+L"));
    keybinds.insert(QStringLiteral("fileTreeSearchAction"), QStringLiteral("Ctrl+Shift+F"));
    keybinds.insert(QStringLiteral("assembleAction"), QStringLiteral("Ctrl+Shift+A"));
    keybinds.insert(QStringLiteral("assembleUpdateChecksumOpenProject64Action"),
                    QStringLiteral("Ctrl+Shift+P"));
    keybinds.insert(QStringLiteral("undoAction"), QStringLiteral("Ctrl+Z"));
    keybinds.insert(QStringLiteral("redoAction"), QStringLiteral("Ctrl+Y"));
    keybinds.insert(QStringLiteral("cutAction"), QStringLiteral("Ctrl+X"));
    keybinds.insert(QStringLiteral("copyAction"), QStringLiteral("Ctrl+C"));
    keybinds.insert(QStringLiteral("pasteAction"), QStringLiteral("Ctrl+V"));
    keybinds.insert(QStringLiteral("selectAllAction"), QStringLiteral("Ctrl+A"));
    keybinds.insert(QStringLiteral("commentOutAction"), QStringLiteral("Ctrl+Shift+C"));

    QJsonObject settings;
    settings.insert(QStringLiteral("applicationFontFamily"), QStringLiteral("JetBrainsMono Nerd Font"));
    settings.insert(QStringLiteral("applicationFontSize"), 12);
    settings.insert(QStringLiteral("editorFontFamily"), QStringLiteral("JetBrainsMono Nerd Font"));
    settings.insert(QStringLiteral("editorFontSize"), 12);
    settings.insert(QStringLiteral("tabWidth"), 4);
    settings.insert(QStringLiteral("theme"), QStringLiteral("System"));
    settings.insert(QStringLiteral("accentColor"), QStringLiteral("#ff4da6"));
    settings.insert(QStringLiteral("keybinds"), keybinds);
    return settings;
}

// Overwrite the initial settings
QJsonObject loadGeneralSettings()
{
    QJsonObject settings = defaultGeneralSettings();
    QFile file(generalSettingsFilePath());
    if (!file.open(QIODevice::ReadOnly))
        return settings;

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return settings;

    const QJsonObject saved = document.object();
    for (const QString &key : {QStringLiteral("applicationFontFamily"),
                                QStringLiteral("applicationFontSize"),
                                QStringLiteral("editorFontFamily"),
                                QStringLiteral("editorFontSize"),
                                QStringLiteral("tabWidth"), QStringLiteral("theme"),
                                QStringLiteral("accentColor")}) {
        if (saved.contains(key))
            settings.insert(key, saved.value(key));
    }
    if (saved.contains(QStringLiteral("fontSize"))) {
        if (!saved.contains(QStringLiteral("applicationFontSize")))
            settings.insert(QStringLiteral("applicationFontSize"), saved.value(QStringLiteral("fontSize")));
        if (!saved.contains(QStringLiteral("editorFontSize")))
            settings.insert(QStringLiteral("editorFontSize"), saved.value(QStringLiteral("fontSize")));
    }

    QJsonObject keybinds = settings.value(QStringLiteral("keybinds")).toObject();
    const QJsonObject savedKeybinds = saved.value(QStringLiteral("keybinds")).toObject();
    for (auto it = savedKeybinds.constBegin(); it != savedKeybinds.constEnd(); ++it) {
        if (it.value().isString() && keybinds.contains(it.key()))
            keybinds.insert(it.key(), it.value());
    }
    const QString savedPreviousTab = savedKeybinds
        .value(QStringLiteral("previousTabAction")).toString();
    if (savedPreviousTab == QStringLiteral("Ctrl+PageUp")
        || savedPreviousTab == QStringLiteral("Ctrl+PageDown")) {
        keybinds.insert(QStringLiteral("previousTabAction"), QStringLiteral("Ctrl+PgUp"));
    }
    const QString savedNextTab = savedKeybinds
        .value(QStringLiteral("nextTabAction")).toString();
    if (savedNextTab == QStringLiteral("Ctrl+PageUp")
        || savedNextTab == QStringLiteral("Ctrl+PageDown")) {
        keybinds.insert(QStringLiteral("nextTabAction"), QStringLiteral("Ctrl+PgDown"));
    }
    settings.insert(QStringLiteral("keybinds"), keybinds);
    return settings;
}

// Self explainatory
bool writeJsonObjectAtomically(const QString &path,
                               const QJsonObject &object,
                               QString *errorMessage)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        if (errorMessage)
            *errorMessage = QObject::tr("Could not create the settings directory.");
        return false;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorMessage)
            *errorMessage = file.errorString();
        return false;
    }
    const QByteArray json = QJsonDocument(object).toJson(QJsonDocument::Indented);
    if (file.write(json) != json.size()) {
        if (errorMessage)
            *errorMessage = file.errorString();
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        if (errorMessage)
            *errorMessage = file.errorString();
        return false;
    }
    return true;
}

bool writeJsonArrayAtomically(const QString &path,
                              const QJsonArray &array,
                              QString *errorMessage)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        if (errorMessage)
            *errorMessage = QObject::tr("Could not create the settings directory.");
        return false;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorMessage)
            *errorMessage = file.errorString();
        return false;
    }
    const QByteArray json = QJsonDocument(array).toJson(QJsonDocument::Indented);
    if (file.write(json) != json.size()) {
        if (errorMessage)
            *errorMessage = file.errorString();
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        if (errorMessage)
            *errorMessage = file.errorString();
        return false;
    }
    return true;
}

// Returns the config file of a given ROM
QString configFilePathForRom(const QString &romFilePath)
{
    const QFileInfo romInfo(romFilePath);
    return romInfo.dir().filePath(romInfo.completeBaseName() + QStringLiteral(".config"));
}

// Returns the backup ROM filename using the current date and time
QString backupRomFileName(const QString &stamp, const QString &romSuffix)
{
    return romSuffix.isEmpty() ? stamp : stamp + QLatin1Char('.') + romSuffix;
}

// Make sure no file of the same name exists
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

// Self explainatory
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

    while (true) {
        const QByteArray chunk = source.read(1024 * 1024);
        if (chunk.isEmpty()) {
            if (source.atEnd() && source.error() == QFileDevice::NoError)
                break;
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

// W constructor (TODO: clean up ts :sob:)
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    generalSettings = loadGeneralSettings();
    macros = loadMacros();
    applyGeneralAppearance();
    ui->setupUi(this);
    updateNetworkManager = new QNetworkAccessManager(this);

    this->setWindowTitle("mipsie");

    MenuBar menuBar(ui, this);
    menuBar.initialize();

    splitter = new Splitter(Qt::Horizontal, this);

    auto *fileTreePane = new Pane;
    auto *fileTree = new FileTree(fileTreePane);
    fileTree->initialize(projectRootPath);
    auto *fileTreeSearchAction = new QAction(tr("Search Files"), this);
    fileTreeSearchAction->setObjectName(QStringLiteral("fileTreeSearchAction"));
    fileTreeSearchAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+F")));
    fileTreeSearchAction->setShortcutContext(Qt::WindowShortcut);
    addAction(fileTreeSearchAction);
    connect(fileTreeSearchAction, &QAction::triggered, fileTree, &FileTree::focusSearch);
    if (QMenu *fileMenu = findChild<QMenu *>(QStringLiteral("fileMenu"))) {
        fileMenu->addAction(fileTreeSearchAction);
    }
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
    editor->setEditorPreferences(
        generalSettings.value(QStringLiteral("editorFontSize")).toInt(12),
        generalSettings.value(QStringLiteral("tabWidth")).toInt(4),
        generalSettings.value(QStringLiteral("theme")).toString(QStringLiteral("System")),
        generalSettings.value(QStringLiteral("editorFontFamily")).toString(QStringLiteral("JetBrainsMono Nerd Font")));
    if (QMenu *fileMenu = findChild<QMenu *>(QStringLiteral("fileMenu"))) {
        for (const QString &name : {QStringLiteral("newTabAction"),
                                    QStringLiteral("closeTabAction"),
                                    QStringLiteral("previousTabAction"),
                                    QStringLiteral("nextTabAction")}) {
            if (QAction *action = editor->findChild<QAction *>(name))
                fileMenu->addAction(action);
        }
    }
    if (QMenu *editMenu = findChild<QMenu *>(QStringLiteral("editMenu"))) {
        for (const QString &name : {QStringLiteral("findAction"),
                                    QStringLiteral("replaceAction"),
                                    QStringLiteral("searchLabelsAction")}) {
            if (QAction *action = editor->findChild<QAction *>(name))
                editMenu->addAction(action);
        }
    }
    editorPane->setContent(editor, PaneKind::Editor);
    editorColumn->addWidget(editorPane);
    splitter->registerPane(editorPane);

    auto *assemblerPane = new Pane;
    auto *assembler = new Assembler(assemblerPane);
    assembler->setRomFilePath(projectRomFilePath.isEmpty()
                                  ? standaloneRomFilePath : projectRomFilePath);
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

    // Initial splits
    splitter->setInitialSizes({250, 1000});
    workspaceSplitter->setInitialSizes({650, 350});
    editorColumn->setInitialSizes({700, 300});
    snippetsColumn->setInitialSizes({550, 450});

    setCentralWidget(splitter);
    qApp->installEventFilter(this);

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
        updateEditActionStates();

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

    applyKeyboardShortcuts();
    QTimer::singleShot(0, this, [this] { startUpdateCheck(false); });
}

bool MainWindow::hasEditTarget() const
{
    return !lastEditTarget.isNull();
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::KeyPress && splitter) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        const Qt::KeyboardModifiers modifiers = keyEvent->modifiers()
                                                & (Qt::ShiftModifier | Qt::ControlModifier
                                                   | Qt::AltModifier | Qt::MetaModifier);
        if (modifiers == Qt::ControlModifier
            && keyEvent->key() >= Qt::Key_1 && keyEvent->key() <= Qt::Key_9) {
            const QKeySequence pressed(Qt::CTRL | keyEvent->key());
            const QList<Pane *> panes = splitter->findChildren<Pane *>();
            for (Pane *pane : panes) {
                QAction *action = pane->findChild<QAction *>(QStringLiteral("focusPaneAction"));
                if (action && action->shortcut() == pressed) {
                    action->trigger();
                    return true;
                }
            }
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::focusEditor()
{
    if (Editor *editor = findActiveEditor()) {
        if (QTextEdit *textEditor = editor->currentEditor())
            textEditor->setFocus(Qt::OtherFocusReason);
    }
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

void MainWindow::toggleCommentOutSelection()
{
    if (!qobject_cast<QTextEdit *>(lastEditTarget.data()))
        return;
    if (Editor *editor = findActiveEditor())
        editor->toggleCommentOut();
}

void MainWindow::applyGeneralAppearance()
{
    const int fontSize = qBound(8,
                                generalSettings.value(QStringLiteral("applicationFontSize")).toInt(12),
                                40);
    const QString theme = generalSettings.value(QStringLiteral("theme"))
                              .toString(QStringLiteral("System"));
    QColor accentColor(generalSettings.value(QStringLiteral("accentColor"))
                           .toString(QStringLiteral("#ff4da6")));
    if (!accentColor.isValid())
        accentColor = QColor(QStringLiteral("#ff4da6"));

    QFont applicationFont = qApp->font();
    applicationFont.setFamily(generalSettings.value(QStringLiteral("applicationFontFamily"))
                                  .toString(QStringLiteral("JetBrainsMono Nerd Font")));
    applicationFont.setPointSize(fontSize);
    qApp->setFont(applicationFont);
    qApp->setProperty("mipsieTheme", theme);
    qApp->setProperty("mipsieAccentColor", accentColor);

    QPalette palette = qApp->style()->standardPalette();
    if (theme == QStringLiteral("Dark")) {
        palette.setColor(QPalette::Window, QColor(QStringLiteral("#252526")));
        palette.setColor(QPalette::WindowText, QColor(QStringLiteral("#eeeeee")));
        palette.setColor(QPalette::Base, QColor(QStringLiteral("#1e1e1e")));
        palette.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#2d2d30")));
        palette.setColor(QPalette::ToolTipBase, QColor(QStringLiteral("#333333")));
        palette.setColor(QPalette::ToolTipText, QColor(QStringLiteral("#eeeeee")));
        palette.setColor(QPalette::Text, QColor(QStringLiteral("#eeeeee")));
        palette.setColor(QPalette::Button, QColor(QStringLiteral("#333333")));
        palette.setColor(QPalette::ButtonText, QColor(QStringLiteral("#eeeeee")));
        palette.setColor(QPalette::BrightText, QColor(Qt::white));
    } else if (theme == QStringLiteral("Light")) {
        palette.setColor(QPalette::Window, QColor(QStringLiteral("#f4f4f4")));
        palette.setColor(QPalette::WindowText, QColor(QStringLiteral("#202124")));
        palette.setColor(QPalette::Base, QColor(Qt::white));
        palette.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#f4f4f4")));
        palette.setColor(QPalette::Text, QColor(QStringLiteral("#202124")));
        palette.setColor(QPalette::Button, QColor(QStringLiteral("#eeeeee")));
        palette.setColor(QPalette::ButtonText, QColor(QStringLiteral("#202124")));
    }
    palette.setColor(QPalette::Highlight, accentColor);
    palette.setColor(QPalette::HighlightedText,
                     accentColor.lightness() > 128 ? QColor(Qt::black) : QColor(Qt::white));
    palette.setColor(QPalette::Link, accentColor);
    qApp->setPalette(palette);

    if (splitter) {
        const auto panes = splitter->findChildren<Pane *>();
        for (Pane *pane : panes)
            pane->update();
    }
}

void MainWindow::applyEditorPreferences()
{
    const int fontSize = generalSettings.value(QStringLiteral("editorFontSize")).toInt(12);
    const int tabWidth = generalSettings.value(QStringLiteral("tabWidth")).toInt(4);
    const QString theme = generalSettings.value(QStringLiteral("theme"))
                              .toString(QStringLiteral("System"));
    const QString fontFamily = generalSettings.value(QStringLiteral("editorFontFamily"))
                                   .toString(QStringLiteral("JetBrainsMono Nerd Font"));
    if (!splitter)
        return;

    const auto editors = splitter->findChildren<Editor *>();
    for (Editor *editor : editors)
        editor->setEditorPreferences(fontSize, tabWidth, theme, fontFamily);

}

void MainWindow::applyKeyboardShortcuts()
{
    const QJsonObject keybinds = generalSettings.value(QStringLiteral("keybinds")).toObject();
    const QStringList actionNames = {
        QStringLiteral("openFileAction"),
        QStringLiteral("openFolderAction"),
        QStringLiteral("openProjectAction"),
        QStringLiteral("openSettingsAction"),
        QStringLiteral("saveFileAction"),
        QStringLiteral("saveFileAsAction"),
        QStringLiteral("closeTabAction"),
        QStringLiteral("previousTabAction"),
        QStringLiteral("nextTabAction"),
        QStringLiteral("newTabAction"),
        QStringLiteral("backupRomAction"),
        QStringLiteral("restoreRomVersionAction"),
        QStringLiteral("findAction"),
        QStringLiteral("replaceAction"),
        QStringLiteral("searchLabelsAction"),
        QStringLiteral("fileTreeSearchAction"),
        QStringLiteral("assembleAction"),
        QStringLiteral("assembleUpdateChecksumOpenProject64Action"),
        QStringLiteral("undoAction"),
        QStringLiteral("redoAction"),
        QStringLiteral("cutAction"),
        QStringLiteral("copyAction"),
        QStringLiteral("pasteAction"),
        QStringLiteral("selectAllAction"),
        QStringLiteral("commentOutAction")
    };
    for (const QString &objectName : actionNames) {
        if (!keybinds.contains(objectName))
            continue;
        const QKeySequence shortcut = QKeySequence::fromString(
            keybinds.value(objectName).toString(), QKeySequence::PortableText);
        const auto actions = findChildren<QAction *>(objectName);
        for (QAction *action : actions)
            action->setShortcut(shortcut);
    }
    updateEditActionStates();
}

void MainWindow::applyMacros()
{
    if (!splitter)
        return;

    const QList<Editor *> editors = splitter->findChildren<Editor *>();
    for (Editor *editor : editors)
        editor->setMacros(macros);
}

void MainWindow::updateEditActionStates()
{
    const bool enabled = hasEditTarget();
    const QStringList editActionNames = {
        QStringLiteral("undoAction"),
        QStringLiteral("redoAction"),
        QStringLiteral("cutAction"),
        QStringLiteral("copyAction"),
        QStringLiteral("pasteAction"),
        QStringLiteral("selectAllAction"),
        QStringLiteral("commentOutAction")
    };
    for (const QString &objectName : editActionNames) {
        if (QAction *action = findChild<QAction *>(objectName))
            action->setEnabled(enabled);
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

bool MainWindow::configureAssemblerRom(Assembler *assembler)
{
    if (!assembler)
        return false;

    QString romFilePath = projectRomFilePath;
    if (romFilePath.isEmpty()) {
        if (!QFileInfo(standaloneRomFilePath).isFile()) {
            standaloneRomFilePath = QFileDialog::getOpenFileName(
                this,
                tr("Select ROM for Assembly"),
                QString(),
                tr("Nintendo 64 ROMs (*.z64 *.n64 *.v64);;All files (*)"));
            if (standaloneRomFilePath.isEmpty())
                return false;
        }
        standaloneRomFilePath = QFileInfo(standaloneRomFilePath).absoluteFilePath();
        romFilePath = standaloneRomFilePath;
    }

    if (!QFileInfo(romFilePath).isFile()) {
        QMessageBox::warning(this, tr("ROM File Not Found"),
                             tr("The ROM file for assembly does not exist:\n%1")
                                 .arg(QDir::toNativeSeparators(romFilePath)));
        return false;
    }

    assembler->setRomFilePath(romFilePath);
    return true;
}

void MainWindow::registerEditor(Editor *editor)
{
    if (!editor)
        return;

    editor->setMacros(macros);

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
        editor->setEditorPreferences(
            generalSettings.value(QStringLiteral("fontSize")).toInt(12),
            generalSettings.value(QStringLiteral("tabWidth")).toInt(4),
            generalSettings.value(QStringLiteral("theme")).toString(QStringLiteral("System")));
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
        assembler->setRomFilePath(projectRomFilePath.isEmpty()
                                      ? standaloneRomFilePath : projectRomFilePath);
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
        connect(console, &LuaConsole::functionRequested,
                this, &MainWindow::luaFunctionRequested);
        pane->setContent(console, PaneKind::LuaConsole);
        for (const QString &line : luaOutputHistory)
            console->appendOutput(line);
        break;
    }
    case PaneKind::CmdConsole: {
        pane->setContent(new CmdConsole(projectRootPath, pane), PaneKind::CmdConsole);
        break;
    }
    case PaneKind::Empty:
        break;
    }

    applyKeyboardShortcuts();
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
        tr("Assembly files (*.asm *.txt);;All files (*)"));
    if (!filePath.isEmpty())
        editor->openFile(filePath);
}

void MainWindow::openRomFile()
{
    if (shuttingDown)
        return;

    const QString initialPath = !standaloneRomFilePath.isEmpty()
        ? QFileInfo(standaloneRomFilePath).absolutePath()
        : (projectRootPath.isEmpty() ? QDir::homePath() : projectRootPath);
    const QString selectedPath = QFileDialog::getOpenFileName(
        this,
        tr("Open ROM for Assembly"),
        initialPath,
        tr("Nintendo 64 ROMs (*.z64 *.n64 *.v64);;All files (*)"));
    if (selectedPath.isEmpty())
        return;

    const QFileInfo romInfo(selectedPath);
    if (!romInfo.isFile()) {
        QMessageBox::warning(this, tr("ROM File Not Found"),
                             tr("The selected ROM file does not exist."));
        return;
    }

    standaloneRomFilePath = QDir::toNativeSeparators(romInfo.absoluteFilePath());
    if (projectRomFilePath.isEmpty() && splitter) {
        const auto assemblers = splitter->findChildren<Assembler *>();
        for (Assembler *assembler : assemblers)
            assembler->setRomFilePath(standaloneRomFilePath);
    }
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

    openProjectFolderPath(selectedFolder);
}

void MainWindow::openCommandLinePath(const QString &path)
{
    if (shuttingDown)
        return;

    const QFileInfo pathInfo(path);
    if (!pathInfo.exists()) {
        QMessageBox::warning(this, tr("Path Not Found"),
                             tr("The specified file or folder does not exist:\n%1")
                                 .arg(QDir::toNativeSeparators(pathInfo.absoluteFilePath())));
        return;
    }

    const QString absolutePath = pathInfo.absoluteFilePath();
    if (pathInfo.isDir()) {
        openProjectFolderPath(absolutePath);
        return;
    }

    if (Editor *editor = findActiveEditor())
        editor->openFile(absolutePath);
}

void MainWindow::openProject()
{
    if (shuttingDown)
        return;

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Open Project"));
    dialog.setMinimumSize(560, 380);
    auto *layout = new QVBoxLayout(&dialog);
    auto *description = new QLabel(tr("Select a recently opened project:"), &dialog);
    layout->addWidget(description);

    auto *projectList = new QListView(&dialog);
    projectList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    projectList->setSelectionMode(QAbstractItemView::SingleSelection);
    auto *model = new QStandardItemModel(projectList);
    const QStringList projects = recentProjectPaths();
    if (projects.isEmpty())
        description->setText(tr("No recent projects. Use File > Open Folder to open a project."));
    for (const QString &path : projects) {
        const QString projectName = QDir(path).dirName();
        auto *item = new QStandardItem(
            QStringLiteral("%1 (%2)").arg(projectName, QDir::toNativeSeparators(path)));
        item->setEditable(false);
        item->setToolTip(QDir::toNativeSeparators(path));
        item->setData(path, RecentProjectPathRole);
        model->appendRow(item);
    }
    projectList->setModel(model);
    layout->addWidget(projectList, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, &dialog);
    auto *openButton = buttons->addButton(tr("Open"), QDialogButtonBox::AcceptRole);
    openButton->setEnabled(model->rowCount() > 0);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(projectList, &QListView::doubleClicked, &dialog,
            [&dialog](const QModelIndex &index) {
        if (index.isValid())
            dialog.accept();
    });
    if (model->rowCount() > 0)
        projectList->setCurrentIndex(model->index(0, 0));

    if (dialog.exec() != QDialog::Accepted)
        return;
    const QModelIndex selectedIndex = projectList->currentIndex();
    if (selectedIndex.isValid())
        openProjectFolderPath(selectedIndex.data(RecentProjectPathRole).toString());
}

QStringList MainWindow::recentProjectPaths() const
{
    QFile recentFile(recentProjectsFilePath());
    if (!recentFile.open(QIODevice::ReadOnly))
        return {};

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(recentFile.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return {};

    QStringList paths;
    const QJsonArray recentProjects = document.object()
        .value(QStringLiteral("recentProjects")).toArray();
    for (const QJsonValue &value : recentProjects) {
        if (!value.isString())
            continue;
        const QString path = QDir::cleanPath(QFileInfo(value.toString()).absoluteFilePath());
        if (!QDir(path).exists()
            || !QFileInfo(QDir(path).filePath(QStringLiteral("mipsie.json"))).isFile())
            continue;

        bool duplicate = false;
        for (const QString &existingPath : paths) {
            if (existingPath.compare(path, Qt::CaseInsensitive) == 0) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate)
            paths.append(path);
        if (paths.size() >= 20)
            break;
    }
    return paths;
}

void MainWindow::rememberRecentProject(const QString &folderPath)
{
    QString normalizedPath = QDir(folderPath).canonicalPath();
    if (normalizedPath.isEmpty())
        normalizedPath = QDir(folderPath).absolutePath();
    normalizedPath = QDir::cleanPath(normalizedPath);

    QStringList paths = recentProjectPaths();
    for (int i = paths.size() - 1; i >= 0; --i) {
        if (paths.at(i).compare(normalizedPath, Qt::CaseInsensitive) == 0)
            paths.removeAt(i);
    }
    paths.prepend(normalizedPath);
    while (paths.size() > 20)
        paths.removeLast();

    QJsonArray recentProjects;
    for (const QString &path : paths)
        recentProjects.append(QDir::fromNativeSeparators(path));
    QJsonObject settings;
    settings.insert(QStringLiteral("recentProjects"), recentProjects);

    const QString settingsPath = recentProjectsFilePath();
    if (!QDir().mkpath(QFileInfo(settingsPath).absolutePath())) {
        QMessageBox::warning(this, tr("Could Not Save Recent Projects"),
                             tr("Could not create the mipsie settings directory."));
        return;
    }

    QSaveFile settingsFile(settingsPath);
    const QByteArray json = QJsonDocument(settings).toJson(QJsonDocument::Indented);
    if (!settingsFile.open(QIODevice::WriteOnly)
        || settingsFile.write(json) != json.size()
        || !settingsFile.commit()) {
        QMessageBox::warning(this, tr("Could Not Save Recent Projects"),
                             tr("Could not save the recent project list: %1")
                                 .arg(settingsFile.errorString()));
    }
}

void MainWindow::openProjectFolderPath(const QString &selectedFolder)
{
    if (shuttingDown)
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
    if (splitter) {
        const auto commandPrompts = splitter->findChildren<CmdConsole *>();
        for (CmdConsole *commandPrompt : commandPrompts)
            commandPrompt->setWorkingDirectory(projectRootPath);
    }
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
    if (QFileInfo(configPath).isFile())
        rememberRecentProject(folderPath);
}

void MainWindow::openSettings()
{
    if (shuttingDown)
        return;

    const QString projectConfigPath = projectRootPath.isEmpty()
        ? QString()
        : QDir(projectRootPath).filePath(QStringLiteral("mipsie.json"));
    const bool hasProjectSettings = !projectConfigPath.isEmpty()
        && QFileInfo(projectConfigPath).isFile();
    QJsonObject projectSettings;
    if (hasProjectSettings) {
        QFile projectFile(projectConfigPath);
        if (!projectFile.open(QIODevice::ReadOnly)) {
            QMessageBox::warning(this, tr("Settings"),
                                 tr("Could not read the current project's mipsie.json: %1")
                                     .arg(projectFile.errorString()));
            return;
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(projectFile.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            QMessageBox::warning(this, tr("Settings"),
                                 tr("The current project's mipsie.json is invalid: %1")
                                     .arg(parseError.errorString()));
            return;
        }
        projectSettings = document.object();
    }

    SettingsDialog dialog(hasProjectSettings ? projectRootPath : QString(),
                          projectRomFilePath,
                          projectSettings,
                          generalSettings,
                          macros,
                          this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    QString errorMessage;
    const QJsonObject updatedGeneralSettings = dialog.generalSettings();
    const QJsonArray updatedMacros = dialog.macros();
    if (!writeJsonObjectAtomically(generalSettingsFilePath(),
                                   updatedGeneralSettings,
                                   &errorMessage)) {
        QMessageBox::warning(this, tr("Settings Not Saved"),
                             tr("Could not save general settings: %1").arg(errorMessage));
        return;
    }

    generalSettings = updatedGeneralSettings;
    applyGeneralAppearance();
    applyEditorPreferences();
    applyKeyboardShortcuts();

    if (!writeJsonArrayAtomically(macrosFilePath(), updatedMacros, &errorMessage)) {
        QMessageBox::warning(this, tr("Macros Not Saved"),
                             tr("General settings were saved, but macros could not be saved: %1")
                                 .arg(errorMessage));
        return;
    }
    macros = updatedMacros;
    applyMacros();

    if (!hasProjectSettings)
        return;

    const QJsonObject updatedProjectSettings = dialog.projectSettings();
    if (!writeJsonObjectAtomically(projectConfigPath,
                                   updatedProjectSettings,
                                   &errorMessage)) {
        QMessageBox::warning(this, tr("Project Settings Not Saved"),
                             tr("General settings were saved, but the project settings could not be saved: %1")
                                 .arg(errorMessage));
        return;
    }

    const QString configuredRomPath = updatedProjectSettings
        .value(QStringLiteral("romFilePath")).toString();
    const QString resolvedRomPath = QDir::isAbsolutePath(configuredRomPath)
        ? configuredRomPath
        : QDir(projectRootPath).absoluteFilePath(configuredRomPath);
    projectRomFilePath = QDir::toNativeSeparators(QFileInfo(resolvedRomPath).absoluteFilePath());
    if (splitter) {
        const auto assemblers = splitter->findChildren<Assembler *>();
        for (Assembler *assembler : assemblers)
            assembler->setRomFilePath(projectRomFilePath);
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

    if (!configureAssemblerRom(assembler))
        return;

    assembler->assemble();
}

void MainWindow::assembleUpdateChecksumAndOpenProject64()
{
    if (shuttingDown || !splitter)
        return;

    Assembler *assembler = splitter->findChild<Assembler *>();
    if (!assembler) {
        QMessageBox::information(this, tr("Assembler Not Open"),
                                 tr("Create an Assembler pane before assembling."));
        return;
    }

    if (!configureAssemblerRom(assembler))
        return;

    assembler->assemble(true);
}

void MainWindow::checkForUpdates()
{
    startUpdateCheck(true);
}

void MainWindow::showAboutDialog()
{
    const QString aboutText = tr("mipsie %1\n\nMade by UncaughtReference\nTesters: placeholder\nSources: hack64.net, asm-editor.specy.app")
                                 .arg(QCoreApplication::applicationVersion());
    QMessageBox::information(this, tr("About mipsie"), aboutText);
}

void MainWindow::openAppDataFolder()
{
    const QString appDataPath = QFileInfo(recentProjectsFilePath()).absolutePath();
    if (!QDir().mkpath(appDataPath)) {
        QMessageBox::warning(this, tr("Open App Data"),
                             tr("Could not create the app data folder:\n%1")
                                 .arg(QDir::toNativeSeparators(appDataPath)));
        return;
    }
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(appDataPath))) {
        QMessageBox::warning(this, tr("Open App Data"),
                             tr("Could not open the app data folder:\n%1")
                                 .arg(QDir::toNativeSeparators(appDataPath)));
    }
}

void MainWindow::startUpdateCheck(bool manual)
{
    if (shuttingDown)
        return;

    if (updateCheckReply) {
        updateCheckWasManual = updateCheckWasManual || manual;
        return;
    }

    QSettings settings;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const QDateTime lastSuccessfulCheck = settings
        .value(QStringLiteral("updates/lastSuccessfulCheckUtc")).toDateTime();
    constexpr qint64 checkIntervalSeconds = 24 * 60 * 60;
    if (!manual && lastSuccessfulCheck.isValid()
        && lastSuccessfulCheck.secsTo(now) >= 0
        && lastSuccessfulCheck.secsTo(now) < checkIntervalSeconds) {
        return;
    }

    QNetworkRequest request(QUrl(QStringLiteral(
        "https://api.github.com/repos/UncaughtReference/mipsie/releases/latest")));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("mipsie/%1").arg(QCoreApplication::applicationVersion()));
    request.setTransferTimeout(10000);

    updateCheckWasManual = manual;
    QNetworkReply *reply = updateNetworkManager->get(request);
    updateCheckReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        updateCheckReply = nullptr;
        const bool manualCheck = updateCheckWasManual;
        updateCheckWasManual = false;
        const int statusCode = reply->attribute(
            QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QNetworkReply::NetworkError networkError = reply->error();
        const QByteArray response = reply->readAll();
        const QString networkErrorText = reply->errorString();
        reply->deleteLater();

        if (statusCode == 404) {
            QSettings().setValue(QStringLiteral("updates/lastSuccessfulCheckUtc"),
                                 QDateTime::currentDateTimeUtc());
            if (manualCheck) {
                QMessageBox::information(this, tr("Check for Updates"),
                                         tr("No published releases were found."));
            }
            return;
        }
        if (networkError != QNetworkReply::NoError || statusCode < 200 || statusCode >= 300) {
            if (manualCheck) {
                QMessageBox::warning(this, tr("Check for Updates"),
                                     tr("Could not check for updates: %1").arg(networkErrorText));
            }
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(response, &parseError);
        const QJsonObject release = document.object();
        QString latestTag = release.value(QStringLiteral("tag_name")).toString().trimmed();
        if (parseError.error != QJsonParseError::NoError || !document.isObject()
            || latestTag.isEmpty()) {
            if (manualCheck) {
                QMessageBox::warning(this, tr("Check for Updates"),
                                     tr("GitHub returned an invalid release response."));
            }
            return;
        }

        if (latestTag.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
            latestTag.remove(0, 1);
        const QVersionNumber latestVersion = QVersionNumber::fromString(latestTag);
        const QVersionNumber currentVersion = QVersionNumber::fromString(
            QCoreApplication::applicationVersion());
        if (latestVersion.isNull() || currentVersion.isNull()) {
            if (manualCheck) {
                QMessageBox::warning(this, tr("Check for Updates"),
                                     tr("Could not compare the installed and published versions."));
            }
            return;
        }

        QSettings().setValue(QStringLiteral("updates/lastSuccessfulCheckUtc"),
                             QDateTime::currentDateTimeUtc());
        const QUrl releaseUrl(release.value(QStringLiteral("html_url")).toString());
        if (latestVersion > currentVersion) {
            QMessageBox prompt(QMessageBox::Information,
                               tr("Update Available"),
                               tr("Mipsie %1 is available. You have version %2.")
                                   .arg(latestTag, QCoreApplication::applicationVersion()),
                               QMessageBox::NoButton,
                               this);
            QPushButton *viewReleaseButton = prompt.addButton(
                tr("View Release"), QMessageBox::AcceptRole);
            prompt.addButton(tr("Later"), QMessageBox::RejectRole);
            prompt.exec();
            if (prompt.clickedButton() == viewReleaseButton && releaseUrl.isValid())
                QDesktopServices::openUrl(releaseUrl);
        } else if (manualCheck) {
            QMessageBox::information(this, tr("Check for Updates"),
                                     tr("You are using the latest version of Mipsie (%1).")
                                         .arg(QCoreApplication::applicationVersion()));
        }
    });
}

void MainWindow::backupRom()
{
    if (shuttingDown)
        return;
    if (projectRomFilePath.isEmpty()) {
        QMessageBox::warning(this, tr("No Project ROM"),
                             tr("Open a project with a configured ROM before creating a backup."));
        return;
    }

    if (splitter) {
        const auto assemblers = splitter->findChildren<Assembler *>();
        for (Assembler *assembler : assemblers) {
            if (assembler->isBusy()) {
                QMessageBox::warning(this, tr("Assembly In Progress"),
                                     tr("Wait for the assembler to finish before backing up the ROM."));
                return;
            }
        }
    }

    const QFileInfo romInfo(projectRomFilePath);
    const QString romPath = romInfo.absoluteFilePath();
    const QString configPath = configFilePathForRom(romPath);
    if (!QFileInfo(romPath).isFile() || !QFileInfo(configPath).isFile()) {
        QMessageBox::warning(this, tr("Backup Failed"),
                             tr("Both the ROM and its matching .config file must exist."));
        return;
    }

    const QString backupDirectoryPath = romInfo.dir().filePath(QStringLiteral("backups"));
    if (!QDir().mkpath(backupDirectoryPath)) {
        QMessageBox::warning(this, tr("Backup Failed"),
                             tr("Could not create the backups directory:\n%1")
                                 .arg(backupDirectoryPath));
        return;
    }

    const QString stamp = uniqueBackupStem(backupDirectoryPath, romInfo.suffix());
    const QDir backupDirectory(backupDirectoryPath);
    const QString backupRomPath = backupDirectory.filePath(
        backupRomFileName(stamp, romInfo.suffix()));
    const QString backupConfigPath = backupDirectory.filePath(stamp + QStringLiteral(".config"));
    QString errorMessage;
    if (!copyFileAtomically(romPath, backupRomPath, &errorMessage)) {
        QMessageBox::warning(this, tr("Backup Failed"),
                             tr("Could not back up the ROM: %1").arg(errorMessage));
        return;
    }
    if (!copyFileAtomically(configPath, backupConfigPath, &errorMessage)) {
        QFile::remove(backupRomPath);
        QMessageBox::warning(this, tr("Backup Failed"),
                             tr("Could not back up the config file: %1").arg(errorMessage));
        return;
    }

    const QDateTime timestamp = QDateTime::fromString(stamp.left(23), backupTimestampFormat);
    QMessageBox::information(this, tr("Backup Created"),
                             tr("ROM and config backup created for %1.")
                                 .arg(timestamp.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))));
}

void MainWindow::restoreRomVersion()
{
    if (shuttingDown)
        return;
    if (projectRomFilePath.isEmpty()) {
        QMessageBox::warning(this, tr("No Project ROM"),
                             tr("Open a project with a configured ROM before restoring a backup."));
        return;
    }

    if (splitter) {
        const auto assemblers = splitter->findChildren<Assembler *>();
        for (Assembler *assembler : assemblers) {
            if (assembler->isBusy()) {
                QMessageBox::warning(this, tr("Assembly In Progress"),
                                     tr("Wait for the assembler to finish before restoring a ROM."));
                return;
            }
        }
    }

    const QFileInfo romInfo(projectRomFilePath);
    const QString romPath = romInfo.absoluteFilePath();
    const QString configPath = configFilePathForRom(romPath);
    if (!QFileInfo(romPath).isFile() || !QFileInfo(configPath).isFile()) {
        QMessageBox::warning(this, tr("Restore Failed"),
                             tr("Both the ROM and its matching .config file must exist."));
        return;
    }

    const QString backupDirectoryPath = romInfo.dir().filePath(QStringLiteral("backups"));
    const QDir backupDirectory(backupDirectoryPath);
    QDialog restoreDialog(this);
    restoreDialog.setWindowTitle(tr("Restore ROM Version"));
    restoreDialog.setMinimumSize(480, 360);
    auto *dialogLayout = new QVBoxLayout(&restoreDialog);
    dialogLayout->addWidget(new QLabel(tr("Choose a date and time to restore:"),
                                       &restoreDialog));

    auto *backupList = new QListView(&restoreDialog);
    backupList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    backupList->setSelectionMode(QAbstractItemView::SingleSelection);
    auto *backupModel = new QStandardItemModel(backupList);
    backupList->setModel(backupModel);

    const QFileInfoList backupFiles = backupDirectory.entryInfoList(
        QDir::Files, QDir::Name | QDir::Reversed);
    for (const QFileInfo &backupInfo : backupFiles) {
        if (backupInfo.suffix().compare(QStringLiteral("config"), Qt::CaseInsensitive) == 0)
            continue;
        if (!romInfo.suffix().isEmpty()
            && backupInfo.suffix().compare(romInfo.suffix(), Qt::CaseInsensitive) != 0)
            continue;

        const QString stamp = backupInfo.completeBaseName();
        const QString pairedConfigPath = backupDirectory.filePath(stamp + QStringLiteral(".config"));
        if (!QFileInfo(pairedConfigPath).isFile())
            continue;

        const QDateTime timestamp = QDateTime::fromString(stamp.left(23), backupTimestampFormat);
        if (!timestamp.isValid())
            continue;
        QString label = timestamp.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
        if (stamp.size() > 23 && stamp.at(23) == QLatin1Char('_'))
            label += QStringLiteral(" (%1)").arg(stamp.mid(24));

        auto *item = new QStandardItem(label);
        item->setEditable(false);
        item->setData(backupInfo.absoluteFilePath(), BackupRomPathRole);
        item->setData(pairedConfigPath, BackupConfigPathRole);
        backupModel->appendRow(item);
    }
    dialogLayout->addWidget(backupList, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, &restoreDialog);
    auto *restoreButton = buttons->addButton(tr("Restore"), QDialogButtonBox::AcceptRole);
    restoreButton->setEnabled(backupModel->rowCount() > 0);
    dialogLayout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &restoreDialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &restoreDialog, &QDialog::accept);
    connect(backupList, &QListView::doubleClicked, &restoreDialog,
            [&restoreDialog](const QModelIndex &index) {
        if (index.isValid())
            restoreDialog.accept();
    });
    if (backupModel->rowCount() > 0)
        backupList->setCurrentIndex(backupModel->index(0, 0));

    if (restoreDialog.exec() != QDialog::Accepted)
        return;
    const QModelIndex selectedIndex = backupList->currentIndex();
    if (!selectedIndex.isValid())
        return;

    const QString selectedRomPath = selectedIndex.data(BackupRomPathRole).toString();
    const QString selectedConfigPath = selectedIndex.data(BackupConfigPathRole).toString();
    const QString brokenDirectoryPath = backupDirectory.filePath(QStringLiteral("brokenROMs"));
    if (!QDir().mkpath(brokenDirectoryPath)) {
        QMessageBox::warning(this, tr("Restore Failed"),
                             tr("Could not create the brokenROMs directory:\n%1")
                                 .arg(brokenDirectoryPath));
        return;
    }

    const QString brokenStamp = uniqueBackupStem(brokenDirectoryPath, romInfo.suffix());
    const QDir brokenDirectory(brokenDirectoryPath);
    const QString brokenRomPath = brokenDirectory.filePath(
        backupRomFileName(brokenStamp, romInfo.suffix()));
    const QString brokenConfigPath = brokenDirectory.filePath(
        brokenStamp + QStringLiteral(".config"));
    QString errorMessage;
    if (!copyFileAtomically(romPath, brokenRomPath, &errorMessage)) {
        QMessageBox::warning(this, tr("Restore Failed"),
                             tr("Could not back up the current ROM: %1").arg(errorMessage));
        return;
    }
    if (!copyFileAtomically(configPath, brokenConfigPath, &errorMessage)) {
        QFile::remove(brokenRomPath);
        QMessageBox::warning(this, tr("Restore Failed"),
                             tr("Could not back up the current config file: %1")
                                 .arg(errorMessage));
        return;
    }

    if (!copyFileAtomically(selectedRomPath, romPath, &errorMessage)) {
        QMessageBox::warning(this, tr("Restore Failed"),
                             tr("Could not restore the selected ROM: %1").arg(errorMessage));
        return;
    }
    if (!copyFileAtomically(selectedConfigPath, configPath, &errorMessage)) {
        QString rollbackError;
        const bool rolledBack = copyFileAtomically(brokenRomPath, romPath, &rollbackError);
        const QString message = rolledBack
            ? tr("Could not restore the selected config file. The original ROM was restored: %1")
                  .arg(errorMessage)
            : tr("Could not restore the selected config file (%1), and rolling back the ROM also failed (%2).")
                  .arg(errorMessage, rollbackError);
        QMessageBox::warning(this, tr("Restore Failed"), message);
        return;
    }

    QMessageBox::information(this, tr("ROM Restored"),
                             tr("ROM and config restored from %1.")
                                 .arg(selectedIndex.data(Qt::DisplayRole).toString()));
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
    if (qApp) {
        qApp->removeEventFilter(this);
        QObject::disconnect(qApp, nullptr, this, nullptr);
        if (splitter) {
            const QList<Pane *> panes = splitter->findChildren<Pane *>();
            for (Pane *pane : panes)
                QObject::disconnect(qApp, nullptr, pane, nullptr);
        }
    }
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    const auto editors = splitter ? splitter->findChildren<Editor *>() : QList<Editor *>();
    const bool hasUnsavedFiles = std::any_of(
        editors.cbegin(), editors.cend(), [](const Editor *editor) {
            return editor->hasUnsavedFiles();
        });

    if (hasUnsavedFiles) {
        QMessageBox prompt(this);
        prompt.setIcon(QMessageBox::Warning);
        prompt.setWindowTitle(tr("Unsaved Files"));
        prompt.setText(tr("There are unsaved files. Save before closing Mipsie?"));
        auto *saveAllButton = prompt.addButton(tr("Save All"), QMessageBox::AcceptRole);
        auto *discardButton = prompt.addButton(tr("Discard"), QMessageBox::DestructiveRole);
        auto *cancelButton = prompt.addButton(QMessageBox::Cancel);
        prompt.setDefaultButton(cancelButton);
        prompt.exec();

        if (prompt.clickedButton() != saveAllButton
            && prompt.clickedButton() != discardButton) {
            event->ignore();
            return;
        }
        if (prompt.clickedButton() == saveAllButton) {
            for (Editor *editor : editors) {
                if (!editor->saveAllModifiedFiles()) {
                    event->ignore();
                    return;
                }
            }
        }
    }

    shuttingDown = true;
    QMainWindow::closeEvent(event);
    if (!event->isAccepted())
        shuttingDown = false;
}
