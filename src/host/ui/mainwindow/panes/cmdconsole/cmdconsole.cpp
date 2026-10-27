#include "cmdconsole.h"

#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QTextCursor>
#include <QVBoxLayout>

CmdConsole::CmdConsole(const QString &workingDirectory, QWidget *parent)
    : QWidget(parent),
      workingDirectory(workingDirectory.isEmpty()
          ? QDir::currentPath() : QFileInfo(workingDirectory).absoluteFilePath()),
      process(new QProcess(this)),
      output(new QPlainTextEdit(this)),
      commandInput(new QLineEdit(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *toolbar = new QHBoxLayout;
    toolbar->addWidget(new QLabel(tr("Windows Command Prompt"), this));
    toolbar->addStretch(1);
    auto *clearButton = new QPushButton(tr("Clear"), this);
    auto *restartButton = new QPushButton(tr("Restart"), this);
    toolbar->addWidget(clearButton);
    toolbar->addWidget(restartButton);
    layout->addLayout(toolbar);

    output->setReadOnly(true);
    output->setMaximumBlockCount(10000);
    layout->addWidget(output, 1);

    commandInput->setPlaceholderText(tr("Enter a command and press Enter"));
    commandInput->setObjectName(QStringLiteral("paneFocusInput"));
    commandInput->setEnabled(false);
    layout->addWidget(commandInput);

    process->setProcessChannelMode(QProcess::MergedChannels);
    process->setWorkingDirectory(this->workingDirectory);
    connect(process, &QProcess::readyRead, this, &CmdConsole::readOutput);
    connect(process, &QProcess::started, this, [this] {
        commandInput->setEnabled(true);
        appendOutput(tr("[Command Prompt started in %1]").arg(process->workingDirectory()));
    });
    connect(process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            commandInput->setEnabled(false);
            appendOutput(tr("[Could not start cmd.exe: %1]").arg(process->errorString()));
        }
    });
    connect(process,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus) {
        commandInput->setEnabled(false);
        appendOutput(tr("[Command Prompt exited with code %1]").arg(exitCode));
    });
    connect(commandInput, &QLineEdit::returnPressed, this, &CmdConsole::runCommand);
    connect(clearButton, &QPushButton::clicked, output, &QPlainTextEdit::clear);
    connect(restartButton, &QPushButton::clicked, this, &CmdConsole::restartShell);

    startShell();
}

void CmdConsole::startShell()
{
    if (process->state() != QProcess::NotRunning)
        return;

    QString commandProcessor = QDir(qEnvironmentVariable("WINDIR"))
                                  .filePath(QStringLiteral("System32/cmd.exe"));
    if (qEnvironmentVariable("WINDIR").isEmpty()
        || !QFileInfo::exists(commandProcessor))
        commandProcessor = QStringLiteral("cmd.exe");

    process->setProgram(commandProcessor);
    process->setArguments({QStringLiteral("/Q"), QStringLiteral("/D")});
    process->setWorkingDirectory(workingDirectory);
    process->start();
}

void CmdConsole::setWorkingDirectory(const QString &directory)
{
    if (directory.isEmpty())
        return;

    workingDirectory = QFileInfo(directory).absoluteFilePath();
    process->setWorkingDirectory(workingDirectory);
    if (process->state() != QProcess::Running)
        return;

    const QString command = QStringLiteral("cd /d \"%1\"").arg(workingDirectory);
    appendOutput(QStringLiteral("> %1").arg(command));
    process->write(command.toLocal8Bit() + QByteArray("\r\n"));
}

void CmdConsole::restartShell()
{
    commandInput->setEnabled(false);
    if (process->state() != QProcess::NotRunning) {
        process->kill();
        process->waitForFinished(1000);
    }
    startShell();
}

void CmdConsole::runCommand()
{
    const QString command = commandInput->text();
    if (command.trimmed().isEmpty() || process->state() != QProcess::Running)
        return;

    appendOutput(QStringLiteral("> %1").arg(command));
    QByteArray input = command.toLocal8Bit();
    input.append("\r\n");
    process->write(input);
    commandInput->clear();
}

void CmdConsole::appendOutput(const QString &text)
{
    QTextCursor cursor(output->document());
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(text);
    cursor.insertBlock();
    output->setTextCursor(cursor);
    output->ensureCursorVisible();
}

void CmdConsole::readOutput()
{
    const QString text = QString::fromLocal8Bit(process->readAll());
    if (text.isEmpty())
        return;

    QTextCursor cursor(output->document());
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(text);
    output->setTextCursor(cursor);
    output->ensureCursorVisible();
}
