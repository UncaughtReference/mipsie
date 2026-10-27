#ifndef LUACONSOLE_H
#define LUACONSOLE_H

#include <QWidget>

class QTextEdit;
class QLineEdit;

class LuaConsole : public QWidget
{
    Q_OBJECT

public:
    explicit LuaConsole(QWidget *parent = nullptr);
    void appendOutput(const QString &text);

signals:
    void functionRequested(const QString &functionName, const QString &arguments);

private:
    QTextEdit *console;
    QLineEdit *functionInput;
};

#endif // LUACONSOLE_H
