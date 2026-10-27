#include "bindkeymap.h"

#include <sol/sol.hpp>

#include <QAction>
#include <QKeySequence>
#include <QMainWindow>
#include <QObject>
#include <QPointer>

#include <stdexcept>
#include <string>

namespace
{
constexpr const char *LuaKeymapActionProperty = "mipsieLuaKeymapAction";
constexpr const char *LuaKeymapShortcutProperty = "mipsieLuaKeymapShortcut";
}

void bindKeymap(sol::state &lua,
                QMainWindow *mainWindow,
                const std::function<void(const QString &)> &output)
{
    sol::table mipsie = lua["mipsie"];
    sol::table keymap = lua.create_table();
    const QPointer<QMainWindow> guardedWindow(mainWindow);
    keymap.set_function("set", [guardedWindow, output](const std::string &shortcutText,
                                                         sol::function callback) {
        if (!guardedWindow)
            throw std::runtime_error("The main window is unavailable.");
        if (!callback.valid())
            throw std::runtime_error("mipsie.keymap.set expects a function callback.");

        const QString shortcut = QString::fromUtf8(
            shortcutText.data(), static_cast<qsizetype>(shortcutText.size()));
        const QKeySequence sequence = QKeySequence::fromString(
            shortcut, QKeySequence::PortableText);
        if (sequence.isEmpty())
            throw std::runtime_error("mipsie.keymap.set expects a valid shortcut.");

        const QString portableShortcut = sequence.toString(QKeySequence::PortableText);
        for (QAction *existingAction : guardedWindow->findChildren<QAction *>()) {
            if (!existingAction->property(LuaKeymapActionProperty).toBool()
                || existingAction->property(LuaKeymapShortcutProperty).toString()
                       != portableShortcut) {
                continue;
            }

            guardedWindow->removeAction(existingAction);
            existingAction->setShortcut(QKeySequence());
            existingAction->deleteLater();
        }

        auto *action = new QAction(
            QObject::tr("Lua keymap: %1").arg(portableShortcut), guardedWindow.data());
        action->setObjectName(QStringLiteral("luaKeymapAction"));
        action->setProperty(LuaKeymapActionProperty, true);
        action->setProperty(LuaKeymapShortcutProperty, portableShortcut);
        action->setShortcut(sequence);
        action->setShortcutContext(Qt::WindowShortcut);
        guardedWindow->addAction(action);

        sol::protected_function protectedCallback(callback);
        QObject::connect(action, &QAction::triggered, guardedWindow.data(),
                         [protectedCallback, output, portableShortcut]() mutable {
            const sol::protected_function_result result = protectedCallback();
            if (result.valid())
                return;

            const sol::error error = result;
            output(QStringLiteral("[ERROR] Lua keymap callback for %1 failed: %2")
                       .arg(portableShortcut, QString::fromUtf8(error.what())));
        });
    });

    mipsie["keymap"] = keymap;
}
