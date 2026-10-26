#ifndef LUARUNTIME_H
#define LUARUNTIME_H

#include "qmainwindow.h"
#pragma once

#include <memory>
#include <QObject>
#include <QString>

class BindingRegistry;
class ScriptExecutor;

namespace sol {
class state;
}

class LuaRuntime final : public QObject
{
    Q_OBJECT

public:
    explicit LuaRuntime(QObject *parent = nullptr);
    ~LuaRuntime() override;

    LuaRuntime(const LuaRuntime &) = delete;
    LuaRuntime &operator=(const LuaRuntime &) = delete;

    bool initialize(QMainWindow* mainWindow);
    bool executeFile(const QString &path);
    bool executeString(
        const QString &source,
        const QString &chunkName = QStringLiteral("=runtime")
        );

    bool isInitialized() const noexcept;

    void shutdown();

signals:
    void scriptError(
        const QString &source,
        const QString &message
        );

    void scriptOutput(const QString &line);

    void initialized();
    void shuttingDown();

private:
    class Private;
    std::unique_ptr<Private> d;
};

#endif // LUARUNTIME_H
