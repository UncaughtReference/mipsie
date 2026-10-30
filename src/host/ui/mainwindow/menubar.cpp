#include "menubar.h"
#include "ui_mainwindow.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDialog>
#include <QCoreApplication>
#include <QFile>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QList>
#include <QMenu>
#include <QKeySequence>
#include <QObject>
#include <QProcess>
#include <QTextBrowser>
#include <QVBoxLayout>

#include <limits>

namespace {

struct AddressMapping
{
    quint32 romStart;
    quint32 romEnd;
    quint32 ramStart;
    quint32 ramEnd;
};

// Fixed ROM-to-RAM mappings listed in the SM64 ROM memory map.
constexpr AddressMapping sm64AddressMappings[] = {
    {0x001000, 0x0E6258, 0x80246000, 0x8032B258},
    {0x0F5580, 0x102D08, 0x80378800, 0x80385F88},
    {0x21F4C0, 0x2577B8, 0x8016F000, 0x801A72F8}
};

QString formatAddress(quint32 address, int digits)
{
    return QStringLiteral("0x%1")
        .arg(QString::number(address, 16).toUpper().rightJustified(digits, QLatin1Char('0')));
}

void convertAddress(MainWindow *parent, bool ramToRom)
{
    bool accepted = false;
    const QString addressText = QInputDialog::getText(
        parent,
        ramToRom ? QObject::tr("RAM Address to ROM Address")
                 : QObject::tr("ROM Address to RAM Address"),
        ramToRom ? QObject::tr("RAM address (hexadecimal):")
                 : QObject::tr("ROM address (hexadecimal):"),
        QLineEdit::Normal, QString(), &accepted).trimmed();
    if (!accepted || addressText.isEmpty())
        return;

    QString digits = addressText;
    if (digits.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive))
        digits.remove(0, 2);
    bool parsed = false;
    const qulonglong parsedAddress = digits.toULongLong(&parsed, 16);
    if (!parsed || parsedAddress > std::numeric_limits<quint32>::max()) {
        QMessageBox::warning(parent, QObject::tr("Invalid Address"),
                             QObject::tr("Enter a valid 32-bit hexadecimal address."));
        return;
    }

    const quint32 address = static_cast<quint32>(parsedAddress);
    constexpr quint32 extendedRamStart = 0x80400000;
    constexpr quint32 extendedRomStart = 0x01200000;
    constexpr quint32 extendedAddressOffset = 0x7F200000;

    if (ramToRom && address >= extendedRamStart) {
        const quint32 converted = address - extendedAddressOffset;

        QApplication::clipboard()->setText(formatAddress(converted, 6));

        QMessageBox::information(
            parent, QObject::tr("Address Conversion"),
            QObject::tr("Address copied to clipboard!")
            );
        return;
    }
    if (!ramToRom && address >= extendedRomStart) {
        if (address > std::numeric_limits<quint32>::max() - extendedAddressOffset) {
            QMessageBox::warning(parent, QObject::tr("Address Out of Range"),
                                 QObject::tr("The converted RAM address exceeds 32 bits."));
            return;
        }
        const quint32 converted = address + extendedAddressOffset;

        QApplication::clipboard()->setText(formatAddress(converted, 8));

        QMessageBox::information(
            parent, QObject::tr("Address Conversion"),
            QObject::tr("Address copied to clipboard!")
            );
        return;
    }

    for (const AddressMapping &mapping : sm64AddressMappings) {
        const quint32 sourceStart = ramToRom ? mapping.ramStart : mapping.romStart;
        const quint32 sourceEnd = ramToRom ? mapping.ramEnd : mapping.romEnd;
        const quint32 targetStart = ramToRom ? mapping.romStart : mapping.ramStart;
        if (address >= sourceStart && address < sourceEnd) {
            const quint32 converted = targetStart + (address - sourceStart);
            const QString result = formatAddress(converted, ramToRom ? 6 : 8);
            QApplication::clipboard()->setText(result);

            QMessageBox::information(
                parent, QObject::tr("Address Conversion"),
                QObject::tr("Address copied to clipboard!")
                );
            return;
        }
    }

    QMessageBox::information(
        parent, QObject::tr("Address Not Mapped"),
        QObject::tr("This address is not in a fixed ROM/RAM mapping in the SM64 memory map. "
                    "Compressed and other data regions may be loaded to dynamic or segmented RAM addresses."));
}

void openMarkdownWindow(MainWindow *parent, const QString &title,
                        const QString &resourcePath) {
    QFile document(resourcePath);
    if (!document.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(parent, QObject::tr("Documentation unavailable"),
                             QObject::tr("Could not open %1.").arg(title));
        return;
    }

    auto *window = new QDialog(parent, Qt::Window);
    window->setAttribute(Qt::WA_DeleteOnClose);
    window->setWindowTitle(title);
    window->resize(900, 700);

    auto *layout = new QVBoxLayout(window);
    auto *viewer = new QTextBrowser(window);
    viewer->setOpenExternalLinks(true);
    viewer->setMarkdown(QString::fromUtf8(document.readAll()));
    layout->addWidget(viewer);
    window->show();
}

} // namespace


MenuBar::MenuBar(Ui::MainWindow *ui, MainWindow *window)
    : ui(ui), window(window) {}

// FUCKING MESS LMFAOOO
// HOW ELSE AM I SUPPOSED TO DO TS
void MenuBar::initialize() {
    MainWindow *mainWindow = window;
    QMenu* fileMenu = new QMenu(QObject::tr("File"), ui->menubar);
    fileMenu->setObjectName(QStringLiteral("fileMenu"));

    QAction *newWindow = new QAction(QObject::tr("New Window"), fileMenu);
    newWindow->setObjectName(QStringLiteral("newWindowAction"));
    fileMenu->addAction(newWindow);
    QObject::connect(newWindow, &QAction::triggered, window, [mainWindow] {
        if (!QProcess::startDetached(QCoreApplication::applicationFilePath(), {})) {
            QMessageBox::warning(mainWindow, QObject::tr("Could Not Open Window"),
                                 QObject::tr("Could not start another mipsie window."));
        }
    });

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
#ifdef Q_OS_LINUX
    assembleAndOpenProject64Action->setVisible(false);
    assembleAndOpenProject64Action->setShortcut(QKeySequence());
#endif
    QObject::connect(assembleAndOpenProject64Action, &QAction::triggered,
                     window, &MainWindow::assembleUpdateChecksumAndOpenProject64);
    ui->menubar->addMenu(assembleMenu);

    QMenu *toolsMenu = new QMenu(QObject::tr("Tools"), ui->menubar);
    QAction *ramToRomAction = toolsMenu->addAction(
        QObject::tr("RAM Address to ROM Address..."));
    ramToRomAction->setObjectName(QStringLiteral("ramAddressToRomAddressAction"));
    QObject::connect(ramToRomAction, &QAction::triggered, window,
                     [mainWindow] { convertAddress(mainWindow, true); });
    QAction *romToRamAction = toolsMenu->addAction(
        QObject::tr("ROM Address to RAM Address..."));
    romToRamAction->setObjectName(QStringLiteral("romAddressToRamAddressAction"));
    QObject::connect(romToRamAction, &QAction::triggered, window,
                     [mainWindow] { convertAddress(mainWindow, false); });
    ui->menubar->addMenu(toolsMenu);

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
    const auto addDocumentationAction = [helpMenu, mainWindow](const QString &text,
                                                                const QString &resource,
                                                                const QString &objectName) {
        QAction *action = helpMenu->addAction(text);
        action->setObjectName(objectName);
        QObject::connect(action, &QAction::triggered, mainWindow,
                         [mainWindow, text, resource] {
            openMarkdownWindow(mainWindow, text, resource);
        });
    };
    addDocumentationAction(QObject::tr("LLM Use"),
                           QStringLiteral(":/docs/llmUse.md"),
                           QStringLiteral("llmUseHelpAction"));
    addDocumentationAction(QObject::tr("Lua API"),
                           QStringLiteral(":/docs/lua.md"),
                           QStringLiteral("luaApiHelpAction"));
    addDocumentationAction(QObject::tr("Source Code"),
                           QStringLiteral(":/docs/source.md"),
                           QStringLiteral("sourceCodeHelpAction"));
    helpMenu->addSeparator();
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
