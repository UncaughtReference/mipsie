#ifndef CMDCONSOLE_H
#define CMDCONSOLE_H

#include <QWidget>

class QLineEdit;
class QPlainTextEdit;
class QProcess;

class CmdConsole : public QWidget
{
    Q_OBJECT

public:
    explicit CmdConsole(const QString &workingDirectory, QWidget *parent = nullptr);
    void setWorkingDirectory(const QString &workingDirectory);

private:
    QString workingDirectory;
    QProcess *process;
    QPlainTextEdit *output;
    QLineEdit *commandInput;

    void startShell();
    void restartShell();
    void runCommand();
    void appendOutput(const QString &text);
    void readOutput();
};

#endif // CMDCONSOLE_H
