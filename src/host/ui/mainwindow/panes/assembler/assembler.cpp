#include "assembler.h"

#include "../editor.h"

#include <QApplication>
#include <QAbstractItemView>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QListView>
#include <QMessageBox>
#include <QModelIndex>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStringList>
#include <QStyle>
#include <QTemporaryFile>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>
#endif

namespace
{
constexpr int SourceLineRole = Qt::UserRole + 1;
}

Assembler::Assembler(QWidget *parent)
    : QWidget(parent),
      ArmipsOutputView(new QListView(this)),
      outputModel(new QStandardItemModel(this)),
      process(new QProcess(this))
{
    ArmipsOutputView->setObjectName(QStringLiteral("ArmipsOutputView"));
    ArmipsOutputView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ArmipsOutputView->setModel(outputModel);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(ArmipsOutputView, 1);

    connect(ArmipsOutputView, &QListView::doubleClicked,
            this, &Assembler::navigateToOutputLine);
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
}

bool Assembler::isBusy() const
{
    return stage != ProcessStage::Idle;
}

void Assembler::assemble(bool openProject64OnSuccess)
{
    if (stage != ProcessStage::Idle)
        return;

    this->openProject64OnSuccess = openProject64OnSuccess;

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
    startArmips(editor->currentEditor()->toPlainText(), rootDirectory);
}

void Assembler::startArmips(const QString &assemblyText, const QString &rootDirectory)
{
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

    standardOutputBuffer.clear();
    standardErrorBuffer.clear();
    stage = ProcessStage::Armips;
    appendOutputLine(tr("Running armips..."));
    process->setWorkingDirectory(QDir::currentPath());
    process->setProgram(QDir::current().filePath(QStringLiteral("tools/armips.exe")));
    process->setArguments({temporaryAsmPath, QStringLiteral("-root"), rootDirectory});
    process->start();
}

void Assembler::startChecksumUpdate()
{
    standardOutputBuffer.clear();
    standardErrorBuffer.clear();
    stage = ProcessStage::Checksum;
    appendOutputLine(tr("Running rn64crc..."));
    process->setWorkingDirectory(QDir::currentPath());
    process->setProgram(QDir::current().filePath(QStringLiteral("tools/rn64crc.exe")));
    process->setArguments({QDir::toNativeSeparators(activeRomFilePath),
                           QStringLiteral("-u")});
    process->start();
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
    const QString toolName = completedStage == ProcessStage::Armips
        ? QStringLiteral("armips")
        : QStringLiteral("rn64crc");
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
            startChecksumUpdate();
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
    appendOutputLine(tr("Focusing Project64 is supported only on Windows."), true);
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
