#include "luaconsole.h"

#include <QTextCursor>
#include <QTextEdit>
#include <QVBoxLayout>

LuaConsole::LuaConsole(QWidget *parent)
    : QWidget(parent),
      console(new QTextEdit(this))
{
    console->setReadOnly(true);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(console);
}

void LuaConsole::appendOutput(const QString &text)
{
    QTextCursor cursor(console->document());
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(text);
    cursor.insertBlock();
    console->setTextCursor(cursor);
    console->ensureCursorVisible();
}
