#include "bindui.h"
#include "../ui/mainwindow/panes/editor/editor.h"

#include <sol/sol.hpp>

#include <QAbstractButton>
#include <QAction>
#include <QComboBox>
#include <QGroupBox>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QTabWidget>
#include <QTextEdit>
#include <QWidget>
#include <QStringList>
#include <QInputDialog>

#include <string>

namespace
{
std::string toStdString(const QString &value)
{
    const QByteArray utf8 = value.toUtf8();
    return std::string(utf8.constData(), static_cast<std::size_t>(utf8.size()));
}

QString toQString(const std::string &value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

QList<QWidget *> directWidgets(QWidget *parent)
{
    return parent ? parent->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly)
                  : QList<QWidget *>();
}

QString widgetId(QWidget *widget, QWidget *root)
{
    if (!widget || !root)
        return QString();

    QStringList segments;
    QWidget *current = widget;
    while (current && current != root) {
        QWidget *parent = current->parentWidget();
        if (!parent)
            return QString();
        const QList<QWidget *> siblings = directWidgets(parent);
        const int index = siblings.indexOf(current);
        QString segment = QString::fromLatin1(current->metaObject()->className());
        segment += QLatin1Char('[') + QString::number(index) + QLatin1Char(']');
        if (!current->objectName().isEmpty())
            segment += QLatin1Char('{') + current->objectName() + QLatin1Char('}');
        segments.prepend(segment);
        current = parent;
    }
    if (current != root)
        return QString();

    QString rootName = QString::fromLatin1(root->metaObject()->className());
    if (!root->objectName().isEmpty())
        rootName += QLatin1Char('{') + root->objectName() + QLatin1Char('}');
    segments.prepend(rootName);
    return segments.join(QLatin1Char('/'));
}

void collectWidgets(QWidget *parent, QList<QWidget *> &widgets)
{
    for (QWidget *child : directWidgets(parent)) {
        widgets.append(child);
        collectWidgets(child, widgets);
    }
}

QWidget *findWidget(QWidget *root, const QString &id)
{
    if (!root || id.isEmpty())
        return nullptr;
    if (root->objectName() == id || widgetId(root, root) == id)
        return root;

    QList<QWidget *> widgets;
    collectWidgets(root, widgets);
    for (QWidget *widget : widgets) {
        if (widget->objectName() == id || widgetId(widget, root) == id)
            return widget;
    }
    return nullptr;
}

QString widgetText(QWidget *widget)
{
    if (auto *button = qobject_cast<QAbstractButton *>(widget))
        return button->text();
    if (auto *label = qobject_cast<QLabel *>(widget))
        return label->text();
    if (auto *lineEdit = qobject_cast<QLineEdit *>(widget))
        return lineEdit->text();
    if (auto *textEdit = qobject_cast<QTextEdit *>(widget))
        return textEdit->toPlainText();
    if (auto *combo = qobject_cast<QComboBox *>(widget))
        return combo->currentText();
    if (auto *group = qobject_cast<QGroupBox *>(widget))
        return group->title();
    return QString();
}

bool setWidgetText(QWidget *widget, const QString &text)
{
    if (auto *button = qobject_cast<QAbstractButton *>(widget)) {
        button->setText(text);
        return true;
    }
    if (auto *label = qobject_cast<QLabel *>(widget)) {
        label->setText(text);
        return true;
    }
    if (auto *lineEdit = qobject_cast<QLineEdit *>(widget)) {
        lineEdit->setText(text);
        return true;
    }
    if (auto *textEdit = qobject_cast<QTextEdit *>(widget)) {
        textEdit->setPlainText(text);
        return true;
    }
    if (auto *group = qobject_cast<QGroupBox *>(widget)) {
        group->setTitle(text);
        return true;
    }
    return false;
}

sol::table widgetDescription(sol::state &lua, QWidget *widget, QWidget *root)
{
    sol::table result = lua.create_table();
    result["id"] = toStdString(widgetId(widget, root));
    result["object_name"] = toStdString(widget->objectName());
    result["class_name"] = widget->metaObject()->className();
    result["text"] = toStdString(widgetText(widget));
    result["visible"] = widget->isVisible();
    result["enabled"] = widget->isEnabled();
    return result;
}

sol::table actionDescription(sol::state &lua, QAction *action)
{
    sol::table result = lua.create_table();
    result["object_name"] = toStdString(action->objectName());
    result["text"] = toStdString(action->text());
    result["shortcut"] = toStdString(action->shortcut().toString(QKeySequence::PortableText));
    result["enabled"] = action->isEnabled();
    result["visible"] = action->isVisible();
    return result;
}
}

void bindUI(sol::state &lua, QMainWindow *mainWindow)
{
    QPointer<QMainWindow> window(mainWindow);
    sol::table mipsie = lua["mipsie"];
    sol::table ui = lua.create_table();

    ui.set_function("create_popup", [&lua, window](const sol::table &requestedStrings) {
        sol::table result = lua.create_table();

        for (qsizetype i = 1; i <= requestedStrings.size(); ++i) {
            const std::string prompt = requestedStrings[i].get<std::string>();
            const QString promptText = QString::fromUtf8(
                prompt.data(), static_cast<qsizetype>(prompt.size()));

            bool ok = false;
            const QString input = QInputDialog::getText(
                window,
                QObject::tr("Lua String Request"),
                promptText,
                QLineEdit::Normal,
                QString(),
                &ok);

            if (!ok)
                break; // Return answers collected so far

            const QByteArray utf8 = input.toUtf8();
            result[i] = std::string(utf8.constData(),
                                    static_cast<std::size_t>(utf8.size()));
        }

        return result;
    });

    ui.set_function("widgets", [&lua, window] {
        sol::table result = lua.create_table();
        if (!window)
            return result;
        QList<QWidget *> widgets{window.data()};
        collectWidgets(window, widgets);
        int index = 1;
        for (QWidget *widget : widgets)
            result[index++] = widgetDescription(lua, widget, window);
        return result;
    });

    ui.set_function("find", [&lua, window](const std::string &id) -> sol::object {
        if (!window)
            return sol::make_object(lua, sol::nil);
        const QString widgetKey = toQString(id);
        QWidget *widget = findWidget(window, widgetKey);
        if (!widget)
            return sol::make_object(lua, sol::nil);

        QPointer<QMainWindow> root(window);
        sol::table result = lua.create_table();
        result["id"] = toStdString(widgetId(widget, window));
        result["object_name"] = toStdString(widget->objectName());
        result["class_name"] = widget->metaObject()->className();
        result.set_function("text", [root, widgetKey] {
            return root ? toStdString(widgetText(findWidget(root, widgetKey))) : std::string();
        });
        result.set_function("set_text", [root, widgetKey](const std::string &text) {
            QWidget *target = root ? findWidget(root, widgetKey) : nullptr;
            return target && setWidgetText(target, toQString(text));
        });
        result.set_function("visible", [root, widgetKey] {
            QWidget *target = root ? findWidget(root, widgetKey) : nullptr;
            return target && target->isVisible();
        });
        result.set_function("set_visible", [root, widgetKey](bool visible) {
            QWidget *target = root ? findWidget(root, widgetKey) : nullptr;
            if (!target)
                return false;
            target->setVisible(visible);
            return true;
        });
        result.set_function("enabled", [root, widgetKey] {
            QWidget *target = root ? findWidget(root, widgetKey) : nullptr;
            return target && target->isEnabled();
        });
        result.set_function("set_enabled", [root, widgetKey](bool enabled) {
            QWidget *target = root ? findWidget(root, widgetKey) : nullptr;
            if (!target)
                return false;
            target->setEnabled(enabled);
            return true;
        });
        result.set_function("focus", [root, widgetKey] {
            QWidget *target = root ? findWidget(root, widgetKey) : nullptr;
            if (!target)
                return false;

            if (auto *editor = qobject_cast<Editor *>(target)) {
                QTextEdit *textEditor = editor->currentEditor();
                if (!textEditor)
                    return false;
                textEditor->setFocus(Qt::OtherFocusReason);
                return true;
            }

            target->setFocus(Qt::OtherFocusReason);
            return true;
        });
        result.set_function("click", [root, widgetKey] {
            QWidget *target = root ? findWidget(root, widgetKey) : nullptr;
            auto *button = qobject_cast<QAbstractButton *>(target);
            if (!button)
                return false;
            button->click();
            return true;
        });
        result.set_function("current_index", [root, widgetKey] {
            QWidget *target = root ? findWidget(root, widgetKey) : nullptr;
            if (auto *tabs = qobject_cast<QTabWidget *>(target))
                return tabs->currentIndex();
            if (auto *combo = qobject_cast<QComboBox *>(target))
                return combo->currentIndex();
            return -1;
        });
        result.set_function("set_current_index", [root, widgetKey](int index) {
            QWidget *target = root ? findWidget(root, widgetKey) : nullptr;
            if (auto *tabs = qobject_cast<QTabWidget *>(target)) {
                if (index < 0 || index >= tabs->count())
                    return false;
                tabs->setCurrentIndex(index);
                return true;
            }
            if (auto *combo = qobject_cast<QComboBox *>(target)) {
                if (index < 0 || index >= combo->count())
                    return false;
                combo->setCurrentIndex(index);
                return true;
            }
            return false;
        });
        return sol::make_object(lua, result);
    });

    ui.set_function("actions", [&lua, window] {
        sol::table result = lua.create_table();
        if (!window)
            return result;
        const QList<QAction *> actions = window->findChildren<QAction *>();
        int index = 1;
        for (QAction *action : actions)
            result[index++] = actionDescription(lua, action);
        return result;
    });

    ui.set_function("find_action", [&lua, window](const std::string &key) -> sol::object {
        if (!window)
            return sol::make_object(lua, sol::nil);
        const QString actionKey = toQString(key);
        const QList<QAction *> actions = window->findChildren<QAction *>();
        QAction *found = nullptr;
        for (QAction *action : actions) {
            if (action->objectName() == actionKey || action->text() == actionKey) {
                found = action;
                break;
            }
        }
        if (!found)
            return sol::make_object(lua, sol::nil);

        sol::table result = lua.create_table();
        result["object_name"] = toStdString(found->objectName());
        result["text"] = toStdString(found->text());
        QPointer<QAction> action(found);
        result.set_function("trigger", [action] {
            if (!action || !action->isEnabled() || !action->isVisible())
                return false;
            action->trigger();
            return true;
        });
        result.set_function("enabled", [action] {
            return action && action->isEnabled();
        });
        result.set_function("set_enabled", [action](bool enabled) {
            if (!action)
                return false;
            action->setEnabled(enabled);
            return true;
        });
        result.set_function("visible", [action] {
            return action && action->isVisible();
        });
        result.set_function("set_visible", [action](bool visible) {
            if (!action)
                return false;
            action->setVisible(visible);
            return true;
        });
        result.set_function("shortcut", [action] {
            return action
                ? toStdString(action->shortcut().toString(QKeySequence::PortableText))
                : std::string();
        });
        result.set_function("set_shortcut", [action](const std::string &shortcut) {
            if (!action)
                return false;
            action->setShortcut(QKeySequence::fromString(
                toQString(shortcut), QKeySequence::PortableText));
            return true;
        });
        return sol::make_object(lua, result);
    });

    ui.set_function("trigger_action", [window](const std::string &name) {
        if (!window)
            return false;
        const QString actionName = toQString(name);
        const QList<QAction *> actions = window->findChildren<QAction *>();
        for (QAction *action : actions) {
            if (action->objectName() == actionName || action->text() == actionName) {
                if (!action->isEnabled() || !action->isVisible())
                    return false;
                action->trigger();
                return true;
            }
        }
        return false;
    });

    mipsie["ui"] = ui;

    sol::table editorApi = lua.create_table();
    editorApi.set_function("current_line", [&lua, window]() -> sol::object {
        if (!window)
            return sol::make_object(lua, sol::nil);
        Editor *editor = window->findChild<Editor *>();
        QTextEdit *textEditor = editor ? editor->currentEditor() : nullptr;
        if (!textEditor)
            return sol::make_object(lua, sol::nil);
        return sol::make_object(lua, toStdString(textEditor->textCursor().block().text()));
    });
    editorApi.set_function("insert_text", [&lua, window](const std::string &text) -> sol::object {
        if (!window)
            return sol::make_object(lua, sol::nil);
        Editor *editor = window->findChild<Editor *>();
        QTextEdit *textEditor = editor ? editor->currentEditor() : nullptr;
        if (!textEditor)
            return sol::make_object(lua, sol::nil);
        textEditor->textCursor().insertText(QString::fromStdString(text));
        return sol::make_object(lua, toStdString(textEditor->textCursor().block().text()));
    });
    editorApi.set_function("current_file_path", [&lua, window]() -> sol::object {
        if (!window)
            return sol::make_object(lua, sol::nil);
        Editor *editor = window->findChild<Editor *>();
        if (!editor)
            return sol::make_object(lua, sol::nil);
        return sol::make_object(lua, editor->currentFilePath().toStdString());
    });
    editorApi.set_function("current_file_title", [&lua, window]() -> sol::object {
        if (!window)
            return sol::make_object(lua, sol::nil);
        Editor *editor = window->findChild<Editor *>();
        if (!editor)
            return sol::make_object(lua, sol::nil);
        return sol::make_object(lua, editor->currentFileTitle().toStdString());
    });
    mipsie["editor"] = editorApi;
}
