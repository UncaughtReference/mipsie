#include "assembler.h"

#include "../editor/editor.h"

#include <QApplication>
#include <QAbstractItemView>
#include <QAction>
#include <QColor>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QListView>
#include <QMessageBox>
#include <QModelIndex>
#include <QMenu>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStringList>
#include <QStyle>
#include <QTemporaryFile>
#include <QVBoxLayout>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef Q_OS_WIN
#include <windows.h>
#include <tlhelp32.h>
#endif

namespace
{
constexpr int SourceLineRole = Qt::UserRole + 1;

QString armipsExecutablePath()
{
#ifdef Q_OS_LINUX
    const QString relativePath = QStringLiteral("tools/armips_linux/armips");
    const QStringList searchRoots = {
        QCoreApplication::applicationDirPath(),
        QDir::currentPath()
    };

    for (const QString &root : searchRoots) {
        QDir directory(root);
        while (directory.exists()) {
            const QString candidate = directory.filePath(relativePath);
            if (QFileInfo(candidate).isFile())
                return QFileInfo(candidate).absoluteFilePath();

            const QString parentPath = QDir::cleanPath(directory.absoluteFilePath(QStringLiteral("..")));
            if (parentPath == directory.absolutePath())
                break;
            directory.setPath(parentPath);
        }
    }
    return {};
#else
    return QDir::current().filePath(QStringLiteral("tools/armips.exe"));
#endif
}
}

Assembler::Assembler(QWidget *parent)
    : QWidget(parent),
      ArmipsOutputView(new QListView(this)),
      outputModel(new QStandardItemModel(this)),
      process(new QProcess(this)),
      romLabel(new QLabel(this))
{
    ArmipsOutputView->setObjectName(QStringLiteral("ArmipsOutputView"));
    ArmipsOutputView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ArmipsOutputView->setModel(outputModel);
    ArmipsOutputView->setContextMenuPolicy(Qt::CustomContextMenu);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->addWidget(romLabel, 0);
    layout->addWidget(ArmipsOutputView, 1);

    connect(ArmipsOutputView, &QListView::doubleClicked,
            this, &Assembler::navigateToOutputLine);
    connect(ArmipsOutputView, &QListView::customContextMenuRequested,
            this, [this](const QPoint &position) {
        const QModelIndex index = ArmipsOutputView->indexAt(position);
        if (!index.isValid())
            return;

        ArmipsOutputView->setCurrentIndex(index);
        QMenu contextMenu(ArmipsOutputView);

        QAction *navigateToErrorAction = nullptr;
        bool validLine = false;
        const int line = index.data(SourceLineRole).toInt(&validLine);
        if (validLine && !(line < 1))
            navigateToErrorAction = contextMenu.addAction(tr("Navigate to error line"));

        QAction *copyAction = contextMenu.addAction(tr("Copy output"));

        QAction *selected = contextMenu.exec(
            ArmipsOutputView->viewport()->mapToGlobal(position));

        if (selected == copyAction)
            QApplication::clipboard()->setText(index.data(Qt::DisplayRole).toString());
        else if (selected == navigateToErrorAction)
            navigateToOutputLine(index);
    });
    connect(process, &QProcess::readyReadStandardOutput, this, [this] {
        appendProcessOutput(false);
    });
    connect(process, &QProcess::readyReadStandardError, this, [this] {
        appendProcessOutput(true);
    });
    connect(process,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
        finishProcess(exitCode, exitStatus == QProcess::CrashExit);
    });
    connect(process, &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            handleFailedToStart();
    });
}

void Assembler::setRomFilePath(const QString &filePath)
{
    configuredRomFilePath = filePath;
    romLabel->setText(QStringLiteral("ROM: %1").arg(filePath));
}

void Assembler::setCreateTemporaryAsmFile(bool enabled)
{
    createTemporaryAsmFile = enabled;
}

void Assembler::setUseArmipsRootOption(bool enabled)
{
    useArmipsRootOption = enabled;
}

void Assembler::setAdditionalArmipsArguments(const QString &arguments)
{
    additionalArmipsArguments = arguments;
}

bool Assembler::isBusy() const
{
    return stage != ProcessStage::Idle;
}

void Assembler::assemble(bool openProject64OnSuccess)
{
    if (stage != ProcessStage::Idle)
        return;

#ifdef Q_OS_WIN
    this->openProject64OnSuccess = openProject64OnSuccess;
#else
    Q_UNUSED(openProject64OnSuccess);
    this->openProject64OnSuccess = false;
#endif

    outputModel->clear();
    sourceEditor.clear();
    sourceBuffer.clear();
    originalSourcePath.clear();
    originalSourceLabel.clear();
    if (configuredRomFilePath.isEmpty()) {
        appendOutputLine(tr("No ROM is configured. Open a project folder with a mipsie.json file."), true);
        return;
    }
    if (!QFileInfo(configuredRomFilePath).isFile()) {
        appendOutputLine(tr("The configured ROM file does not exist: %1")
                             .arg(configuredRomFilePath), true);
        return;
    }

    Editor *editor = window()->findChild<Editor *>();
    if (!editor || !editor->currentEditor())
    {
        appendOutputLine(tr("No editor pane is open."), true);
        return;
    }

    sourceEditor = editor;
    sourceBuffer = editor->currentEditor();
    activeRomFilePath = configuredRomFilePath;
    originalSourcePath = editor->currentFilePath();
    originalSourceLabel = originalSourcePath.isEmpty()
                              ? editor->currentFileTitle()
                              : originalSourcePath;
    const QString originalFilePath = editor->currentFilePath();
    const QString rootDirectory = originalFilePath.isEmpty()
                                      ? QDir::currentPath()
                                      : QFileInfo(originalFilePath).absolutePath();
    if (!createTemporaryAsmFile) {
        if (originalFilePath.isEmpty() || !QFileInfo(originalFilePath).isFile()) {
            appendOutputLine(tr("Save the current ASM file before assembling without a temporary file."), true);
            return;
        }
        if (sourceBuffer->document()->isModified() && !editor->saveCurrentFile()) {
            appendOutputLine(tr("Assembly cancelled because the ASM file could not be saved."), true);
            return;
        }
    }
    startArmips(editor->currentEditor()->toPlainText(), rootDirectory);
}

void Assembler::startArmips(const QString &assemblyText, const QString &rootDirectory)
{
    QString asmFilePath;
    sourceLineOffset = 0;
    temporaryAsmPath.clear();
    if (createTemporaryAsmFile) {
        QTemporaryFile temporaryFile(QDir::tempPath() + QStringLiteral("/mipsie-XXXXXX.asm"));
        temporaryFile.setAutoRemove(false);
        if (!temporaryFile.open()) {
            appendOutputLine(tr("Could not create temporary ASM file: %1")
                                 .arg(temporaryFile.errorString()), true);
            return;
        }

        temporaryAsmPath = temporaryFile.fileName();
        const QString sourcePrefix = QStringLiteral(
            "MIPSIE_ROM_FILENAME equ \"%1\"\n"
            "MIPSIE_FILE_POS equ 0x00000000\n"
            "\n"
            ".open MIPSIE_ROM_FILENAME, MIPSIE_FILE_POS\n"
            "\n"
            ".n64\n")
            .arg(QDir::toNativeSeparators(activeRomFilePath));
        sourceLineOffset = sourcePrefix.count(QLatin1Char('\n'));
        const QString temporarySource = sourcePrefix + assemblyText + QStringLiteral("\n.close\n");
        const QByteArray encodedSource = temporarySource.toUtf8();
        if (temporaryFile.write(encodedSource) != encodedSource.size()) {
            appendOutputLine(tr("Could not write temporary ASM file: %1")
                                 .arg(temporaryFile.errorString()), true);
            temporaryFile.close();
            removeTemporaryAsm();
            return;
        }
        temporaryFile.close();
        asmFilePath = temporaryAsmPath;
    } else {
        asmFilePath = originalSourcePath;
    }

    standardOutputBuffer.clear();
    standardErrorBuffer.clear();
    stage = ProcessStage::Armips;
    appendOutputLine(tr("Running armips..."));
    process->setWorkingDirectory(QDir::currentPath());
    const QString armipsPath = armipsExecutablePath();
    if (armipsPath.isEmpty()) {
        appendOutputLine(tr("Could not find tools/armips_linux/armips beside the Mipsie executable or in a parent folder."), true);
        removeTemporaryAsm();
        stage = ProcessStage::Idle;
        return;
    }
    process->setProgram(armipsPath);
    QStringList arguments{asmFilePath};
    if (useArmipsRootOption)
        arguments << QStringLiteral("-root") << rootDirectory;
    arguments.append(QProcess::splitCommand(additionalArmipsArguments));
    process->setArguments(arguments);
    process->start();
}

bool Assembler::updateChecksum()
{
    QFile romFile(activeRomFilePath);
    if (!romFile.open(QIODevice::ReadWrite)) {
        appendOutputLine(tr("Could not open ROM to update its checksum: %1")
                             .arg(romFile.errorString()), true);
        return false;
    }

    const QByteArray romData = romFile.readAll();
    constexpr quint32 checksumStart = 0x1000;
    constexpr quint32 checksumEnd = 0x101000;
    if (romFile.error() != QFileDevice::NoError
        || static_cast<quint64>(romData.size()) < checksumEnd) {
        appendOutputLine(tr("ROM is too small or could not be read for checksum calculation."), true);
        return false;
    }

    constexpr quint32 seed = 0xF8CA4DDC;
    quint32 t1 = seed;
    quint32 t2 = seed;
    quint32 t3 = seed;
    quint32 t4 = seed;
    quint32 t5 = seed;
    quint32 t6 = seed;

    const auto readWord = [&romData](quint32 offset) {
        const quint32 b0 = static_cast<quint8>(static_cast<unsigned char>(romData.at(offset)));
        const quint32 b1 = static_cast<quint8>(static_cast<unsigned char>(romData.at(offset + 1)));
        const quint32 b2 = static_cast<quint8>(static_cast<unsigned char>(romData.at(offset + 2)));
        const quint32 b3 = static_cast<quint8>(static_cast<unsigned char>(romData.at(offset + 3)));
        return (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
    };

    for (quint32 offset = checksumStart; offset < checksumEnd; offset += 4) {
        const quint32 d = readWord(offset);
        if (t6 + d < t6)
            ++t4;

        t6 += d;
        t3 ^= d;

        const quint32 shift = d & 0x1F;
        const quint32 rotated = shift == 0
            ? d
            : (d << shift) | (d >> (32 - shift));

        t5 += rotated;
        if (t2 > d)
            t2 ^= rotated;
        else
            t2 ^= t6 ^ d;

        t1 += t5 ^ d;
    }

    const quint32 crc1 = t6 ^ t4 ^ t3;
    const quint32 crc2 = t5 ^ t2 ^ t1;
    const char checksum[8] = {
        static_cast<char>((crc1 >> 24) & 0xFF),
        static_cast<char>((crc1 >> 16) & 0xFF),
        static_cast<char>((crc1 >> 8) & 0xFF),
        static_cast<char>(crc1 & 0xFF),
        static_cast<char>((crc2 >> 24) & 0xFF),
        static_cast<char>((crc2 >> 16) & 0xFF),
        static_cast<char>((crc2 >> 8) & 0xFF),
        static_cast<char>(crc2 & 0xFF)
    };

    if (!romFile.seek(0x10)
        || romFile.write(checksum, sizeof(checksum)) != sizeof(checksum)
        || !romFile.flush()) {
        appendOutputLine(tr("Could not write the updated ROM checksum: %1")
                             .arg(romFile.errorString()), true);
        return false;
    }

    return true;
}

void Assembler::appendProcessOutput(bool standardError)
{
    const QByteArray bytes = standardError
        ? process->readAllStandardError()
        : process->readAllStandardOutput();
    QString &buffer = standardError ? standardErrorBuffer : standardOutputBuffer;
    buffer.append(QString::fromLocal8Bit(bytes));

    int newline = -1;
    while ((newline = buffer.indexOf(QLatin1Char('\n'))) >= 0) {
        QString line = buffer.left(newline);
        if (line.endsWith(QLatin1Char('\r')))
            line.chop(1);
        appendOutputLine(line, standardError);
        buffer.remove(0, newline + 1);
    }
}

void Assembler::flushProcessOutput()
{
    if (!standardOutputBuffer.isEmpty())
        appendOutputLine(standardOutputBuffer);
    if (!standardErrorBuffer.isEmpty())
        appendOutputLine(standardErrorBuffer, true);
    standardOutputBuffer.clear();
    standardErrorBuffer.clear();
}

void Assembler::appendOutputLine(const QString &line, bool isError)
{
    const int temporaryLine = sourceLineForOutput(line);
    const int sourceLine = temporaryLine > sourceLineOffset
        ? temporaryLine - sourceLineOffset
        : -1;
    QString displayLine = line;
    if (!temporaryAsmPath.isEmpty() && !originalSourceLabel.isEmpty()) {
        const QString normalizedTemporaryPath = QDir::fromNativeSeparators(temporaryAsmPath);
        const QString nativeTemporaryPath = QDir::toNativeSeparators(temporaryAsmPath);
        displayLine.replace(temporaryAsmPath, originalSourceLabel, Qt::CaseInsensitive);
        displayLine.replace(normalizedTemporaryPath, originalSourceLabel, Qt::CaseInsensitive);
        displayLine.replace(nativeTemporaryPath, originalSourceLabel, Qt::CaseInsensitive);
        displayLine.replace(QFileInfo(temporaryAsmPath).fileName(),
                            originalSourceLabel, Qt::CaseInsensitive);
    }
    if (sourceLine > 0 && temporaryLine != sourceLine && !originalSourceLabel.isEmpty()) {
        const int pathIndex = displayLine.indexOf(originalSourceLabel, 0, Qt::CaseInsensitive);
        if (pathIndex >= 0) {
            static const QRegularExpression linePattern(QStringLiteral("\\s*\\]?\\s*\\((\\d+)\\)"));
            const auto match = linePattern.match(displayLine, pathIndex + originalSourceLabel.size());
            if (match.hasMatch())
                displayLine.replace(match.capturedStart(1), match.capturedLength(1),
                                    QString::number(sourceLine));
        }
    }

    auto *item = new QStandardItem(displayLine);
    if (sourceLine > 0)
        item->setData(sourceLine, SourceLineRole);
    const bool errorLine = isError || sourceLine > 0
        || line.contains(QStringLiteral("error"), Qt::CaseInsensitive);
    if (errorLine) {
        item->setForeground(QColor(QStringLiteral("#ff5c5c")));
        item->setIcon(style()->standardIcon(QStyle::SP_MessageBoxCritical));
    }
    outputModel->appendRow(item);
    ArmipsOutputView->scrollToBottom();
}

int Assembler::sourceLineForOutput(const QString &line) const
{
    if (stage != ProcessStage::Armips)
        return -1;

    QStringList sourcePaths;
    if (!temporaryAsmPath.isEmpty()) {
        sourcePaths << temporaryAsmPath
                    << QDir::fromNativeSeparators(temporaryAsmPath)
                    << QDir::toNativeSeparators(temporaryAsmPath)
                    << QFileInfo(temporaryAsmPath).fileName();
    }
    if (!originalSourcePath.isEmpty())
        sourcePaths << originalSourcePath;

    static const QRegularExpression linePattern(QStringLiteral("\\s*\\]?\\s*\\((\\d+)\\)"));
    for (const QString &path : sourcePaths) {
        if (path.isEmpty())
            continue;
        const int pathIndex = line.indexOf(path, 0, Qt::CaseInsensitive);
        if (pathIndex < 0)
            continue;
        const auto match = linePattern.match(line, pathIndex + path.size());
        if (match.hasMatch())
            return match.captured(1).toInt();
    }
    return -1;
}

void Assembler::navigateToOutputLine(const QModelIndex &index)
{
    bool validLine = false;
    const int line = index.data(SourceLineRole).toInt(&validLine);
    if (!validLine || line < 1)
        return;

    Editor *editor = sourceEditor.data();
    QTextEdit *buffer = sourceBuffer.data();
    if (!editor)
        editor = window()->findChild<Editor *>();

    if (editor && !originalSourcePath.isEmpty()
        && editor->openFile(originalSourcePath)) {
        editor->goToLine(line);
        return;
    }

    if (editor && buffer && editor->goToLine(line, buffer))
        return;

    if (originalSourcePath.isEmpty())
        return;

    QList<Editor *> editors = window()->findChildren<Editor *>();
    if (editor) {
        editors.removeAll(editor);
        editors.prepend(editor);
    }
    for (Editor *candidate : editors) {
        if (candidate->openFile(originalSourcePath)) {
            candidate->goToLine(line);
            return;
        }
    }
}

void Assembler::finishProcess(int exitCode, bool crashed)
{
    appendProcessOutput(false);
    appendProcessOutput(true);
    flushProcessOutput();
    const ProcessStage completedStage = stage;
    const QString toolName = QStringLiteral("armips");
    const bool succeeded = exitCode == 0 && !crashed;

    QString status = crashed
        ? tr("%1 crashed (exit code %2)").arg(toolName).arg(exitCode)
        : tr("%1 exited with status code %2").arg(toolName).arg(exitCode);
    if (succeeded) {
        auto *item = new QStandardItem(status);
        item->setForeground(QColor(QStringLiteral("#69d17d")));
        item->setIcon(style()->standardIcon(QStyle::SP_DialogApplyButton));
        outputModel->appendRow(item);
        ArmipsOutputView->scrollToBottom();
    } else {
        appendOutputLine(status, true);
    }

    if (completedStage == ProcessStage::Armips) {
        removeTemporaryAsm();
        if (succeeded) {
            appendOutputLine(tr("Updating ROM checksum..."));
            const bool checksumUpdated = updateChecksum();
            if (checksumUpdated) {
                auto *item = new QStandardItem(tr("ROM checksum updated."));
                item->setForeground(QColor(QStringLiteral("#69d17d")));
                item->setIcon(style()->standardIcon(QStyle::SP_DialogApplyButton));
                outputModel->appendRow(item);
                ArmipsOutputView->scrollToBottom();
            } else {
                appendOutputLine(tr("Assembly succeeded, but the ROM checksum could not be updated."), true);
            }

            stage = ProcessStage::Idle;
            if (checksumUpdated && openProject64OnSuccess)
                focusProject64();
            openProject64OnSuccess = false;
        } else {
            appendOutputLine(tr("Checksum update skipped because armips failed."));
            stage = ProcessStage::Idle;
            openProject64OnSuccess = false;
        }
        return;
    }

    stage = ProcessStage::Idle;
    if (succeeded && openProject64OnSuccess)
        focusProject64();
    openProject64OnSuccess = false;
}

void Assembler::handleFailedToStart()
{
    flushProcessOutput();
    appendOutputLine(tr("Could not start %1: %2")
                         .arg(QFileInfo(process->program()).fileName(), process->errorString()), true);
    removeTemporaryAsm();
    stage = ProcessStage::Idle;
    openProject64OnSuccess = false;
}

bool Assembler::focusProject64()
{
#ifdef Q_OS_WIN
    struct WindowSearch
    {
        DWORD processId = 0;
        HWND window = nullptr;
    };

    const auto findWindow = [](HWND window, LPARAM parameter) -> BOOL {
        auto *search = reinterpret_cast<WindowSearch *>(parameter);
        DWORD processId = 0;
        GetWindowThreadProcessId(window, &processId);
        if (processId == search->processId && IsWindowVisible(window)
            && GetWindow(window, GW_OWNER) == nullptr) {
            search->window = window;
            return FALSE;
        }
        return TRUE;
    };

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        appendOutputLine(tr("Could not search for Project64.exe."), true);
        return false;
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    WindowSearch search;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (QString::fromWCharArray(entry.szExeFile).compare(
                    QStringLiteral("Project64.exe"), Qt::CaseInsensitive) != 0)
                continue;
            search.processId = entry.th32ProcessID;
            EnumWindows(findWindow, reinterpret_cast<LPARAM>(&search));
            if (search.window)
                break;
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);

    if (!search.window) {
        appendOutputLine(tr("Project64.exe is not running or has no visible window."), true);
        return false;
    }

    ShowWindow(search.window, SW_RESTORE);
    if (SetForegroundWindow(search.window)) {
        appendOutputLine(tr("Focused Project64."));
        return true;
    }

    FlashWindow(search.window, TRUE);
    appendOutputLine(tr("Windows did not allow Project64 to take focus."), true);
    return false;
#else
    return false;
#endif
}

void Assembler::removeTemporaryAsm()
{
    if (temporaryAsmPath.isEmpty())
        return;
    QFile::remove(temporaryAsmPath);
    temporaryAsmPath.clear();
}
