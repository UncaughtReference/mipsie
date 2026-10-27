#include "luaruntime.h"

#include "../bindings/bindingregistry.h"

#include <third_party/sol/sol.hpp>

#include <QThread>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class LuaRuntime::Private {
public:
    sol::state lua;
    QNetworkAccessManager *networkManager = nullptr;
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
        d->networkManager = new QNetworkAccessManager(this);
        BindingRegistry::registerAll(d->lua, mainWindow, d->networkManager, output);

        d->lua["mipsie"]["_output"] = [output](const std::string &line) {
            output(QString::fromStdString(line));
        };
        d->lua.script(R"(
            local emit = mipsie._output
            print = function(...)
                local values = {}
                for i = 1, select("#", ...) do
                    values[i] = tostring(select(i, ...))
                end
                emit(table.concat(values, "\t"))
            end
        )");

        sol::table mipsie = d->lua["mipsie"];
        sol::table fs = mipsie["fs"];

        qDebug() << "mipsie.fs exists:" << mipsie["fs"].valid();
        qDebug() << "mipsie.fs.current_dir exists:"
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

    if (d->networkManager) {
        const auto replies = d->networkManager->findChildren<QNetworkReply *>();
        for (QNetworkReply *reply : replies) {
            QObject::disconnect(reply, nullptr, d->networkManager, nullptr);
            reply->abort();
        }
        delete d->networkManager;
        d->networkManager = nullptr;
    }

    d->initialized = false;
}
