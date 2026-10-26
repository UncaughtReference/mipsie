#include "luaruntime.h"

#include "../bindings/bindingregistry.h"

#include <third_party/sol/sol.hpp>

#include <QThread>
#include <QDebug>

class LuaRuntime::Private {
public:
    sol::state lua;
    bool initialized = false;
    bool shuttingDown = false;
    QThread *ownerThread = nullptr;
};

LuaRuntime::LuaRuntime(QObject *parent)
    : QObject(parent),
    d(std::make_unique<Private>()) {
    d->ownerThread = QThread::currentThread();
}

LuaRuntime::~LuaRuntime() {
    shutdown();
}

bool LuaRuntime::initialize(QMainWindow* mainWindow)
{
    Q_ASSERT(QThread::currentThread() == d->ownerThread);

    if (d->initialized)
        return true;

    try {
        d->lua.open_libraries(
            sol::lib::base,
            sol::lib::coroutine,
            sol::lib::debug,
            sol::lib::io,
            sol::lib::math,
            sol::lib::os,
            sol::lib::package,
            sol::lib::string,
            sol::lib::table,
            sol::lib::utf8
            );

        // Register all generic host APIs.
        const auto output = [this](const QString &line) { emit scriptOutput(line); };
        BindingRegistry::registerAll(d->lua, mainWindow, output);

        d->lua["host"]["_output"] = [output](const std::string &line) {
            output(QString::fromStdString(line));
        };
        d->lua.script(R"(
            local emit = host._output
            print = function(...)
                local values = {}
                for i = 1, select("#", ...) do
                    values[i] = tostring(select(i, ...))
                end
                emit(table.concat(values, "\t"))
            end
        )");

        sol::table host = d->lua["host"];
        sol::table fs = host["fs"];

        qDebug() << "host.fs exists:" << host["fs"].valid();
        qDebug() << "host.fs.current_dir exists:"
                 << fs["current_dir"].valid();

        d->initialized = true;
        emit initialized();
        return true;
    }
    catch (const std::exception &e) {
        emit scriptError(
            QStringLiteral("runtime"),
            QString::fromUtf8(e.what())
            );

        return false;
    }
}

bool LuaRuntime::executeFile(const QString &path) {
    Q_ASSERT(QThread::currentThread() == d->ownerThread);

    if(!d->initialized || d->shuttingDown) return false;

    const QByteArray encodedPath = path.toUtf8();

    sol::protected_function_result result =
        d->lua.safe_script_file(
        encodedPath.constData(),
        sol::script_pass_on_error
        );

    if(!result.valid()) {
        sol::error error = result;

        emit scriptError(
            path,
            QString::fromUtf8(error.what())
            );

        return false;
    }

    return true;
}

bool LuaRuntime::executeString(
    const QString &source,
    const QString &chunkName
    ) {
    Q_ASSERT(QThread::currentThread() == d->ownerThread);

    if(!d->initialized || d->shuttingDown) return false;

    sol::protected_function_result result =
        d->lua.safe_script(
        source.toUtf8().constData(),
        sol::script_pass_on_error,
        chunkName.toUtf8().constData()
        );

    if(!result.valid()) {
        sol::error error = result;

        emit scriptError(
            chunkName,
            QString::fromUtf8(error.what())
            );

        return false;
    }

    return true;
}

bool LuaRuntime::isInitialized() const noexcept {
    return d->initialized;
}

void LuaRuntime::shutdown() {
    if(!d || d->shuttingDown) return;

    d->shuttingDown = true;
    emit shuttingDown();

    d->lua = sol::state();
    d->initialized = false;
}
