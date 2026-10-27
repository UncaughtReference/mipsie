#include "bindapplication.h"

#include <sol/sol.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QSysInfo>
#include <QVersionNumber>

#include <string>

void bindApplication(sol::state& lua)
{
    sol::table mipsie = lua["mipsie"];
    sol::table app = lua.create_table();

    // Application metadata
    app.set_function("name", []() -> std::string {
        return QCoreApplication::applicationName().toStdString();
    });

    app.set_function("version", []() -> std::string {
        return QCoreApplication::applicationVersion().toStdString();
    });

    // Platform information
    app.set_function("architecture", []() -> std::string {
        return QSysInfo::currentCpuArchitecture().toStdString();
    });

    // Qt version
    app.set_function("qt_version", []() -> std::string {
        return qVersion();
    });

    // Application lifecycle
    app.set_function("quit", []() {
        QCoreApplication::quit();
    });

    mipsie["app"] = app;
}
