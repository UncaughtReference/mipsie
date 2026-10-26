#ifndef LUACONSOLE_H
#define LUACONSOLE_H

#include <QWidget>

class QTextEdit;

class LuaConsole : public QWidget
{
    Q_OBJECT

public:
    explicit LuaConsole(QWidget *parent = nullptr);
    void appendOutput(const QString &text);

private:
    QTextEdit *console;
};

#endif // LUACONSOLE_H
