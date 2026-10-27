#include "luaconsole.h"

#include <QLineEdit>
#include <QRegularExpression>
#include <QStringList>
#include <QTextCursor>
#include <QTextEdit>
#include <QVBoxLayout>

namespace
{
bool parseFunctionCall(const QString &input, QString *functionName, QString *arguments)
{
    static const QRegularExpression callPattern(
        QStringLiteral("^([A-Za-z_][A-Za-z0-9_]*)\\s*(?:\\((.*)\\))?$"));
    const QRegularExpressionMatch match = callPattern.match(input.trimmed());
    if (!match.hasMatch())
        return false;

    *functionName = match.captured(1);
    const QString rawArguments = match.captured(2);
    QStringList parsedArguments;
    int position = 0;
    while (position < rawArguments.size()) {
        while (position < rawArguments.size() && rawArguments.at(position).isSpace())
            ++position;
        if (position >= rawArguments.size())
            return false;

        const int tokenStart = position;
        const QChar first = rawArguments.at(position);
        if (first == QLatin1Char('\'') || first == QLatin1Char('"')) {
            ++position;
            bool closed = false;
            while (position < rawArguments.size()) {
                const QChar character = rawArguments.at(position);
                if (character == QLatin1Char('\\')) {
                    if (position + 1 >= rawArguments.size())
                        return false;
                    position += 2;
                } else if (character == first) {
                    ++position;
                    closed = true;
                    break;
                } else if (character == QLatin1Char('\n')
                           || character == QLatin1Char('\r')) {
                    return false;
                } else {
                    ++position;
                }
            }
            if (!closed)
                return false;
            parsedArguments.append(rawArguments.mid(tokenStart, position - tokenStart));
        } else {
            while (position < rawArguments.size()
                   && rawArguments.at(position) != QLatin1Char(','))
                ++position;
            const QString token = rawArguments.mid(tokenStart, position - tokenStart).trimmed();
            static const QRegularExpression numberPattern(
                QStringLiteral("^[+-]?(?:[0-9]+(?:\\.[0-9]*)?|\\.[0-9]+)(?:[eE][+-]?[0-9]+)?$"));
            if (token != QStringLiteral("true") && token != QStringLiteral("false")
                && token != QStringLiteral("nil")
                && !numberPattern.match(token).hasMatch()) {
                return false;
            }
            parsedArguments.append(token);
        }

        while (position < rawArguments.size() && rawArguments.at(position).isSpace())
            ++position;
        if (position == rawArguments.size())
            break;
        if (rawArguments.at(position) != QLatin1Char(','))
            return false;
        ++position;
        if (position == rawArguments.size())
            return false;
    }

    *arguments = parsedArguments.join(QStringLiteral(", "));
    return true;
}
}

LuaConsole::LuaConsole(QWidget *parent)
    : QWidget(parent),
      console(new QTextEdit(this)),
      functionInput(new QLineEdit(this))
{
    console->setReadOnly(true);
    functionInput->setPlaceholderText(
        tr("Call init.lua function (e.g. foo())"));
    functionInput->setObjectName(QStringLiteral("paneFocusInput"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(console, 1);
    layout->addWidget(functionInput);
    connect(functionInput, &QLineEdit::returnPressed, this, [this] {
        const QString input = functionInput->text().trimmed();
        if (input.isEmpty())
            return;
        QString functionName;
        QString arguments;
        if (!parseFunctionCall(input, &functionName, &arguments)) {
            appendOutput(tr("[Lua Error] Use a function name followed by optional simple arguments: numbers, quoted strings, true, false, or nil."));
            functionInput->selectAll();
            return;
        }
        appendOutput(QStringLiteral("> %1(%2)").arg(functionName, arguments));
        functionInput->clear();
        emit functionRequested(functionName, arguments);
    });
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
