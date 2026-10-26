#include "bindapplication.h"

#include <sol/sol.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QSysInfo>
#include <QVersionNumber>

#include <string>

void bindApplication(sol::state& lua)
{
    sol::table host = lua["host"];
    sol::table app = lua.create_table();

    // Application metadata
    app.set_function("name", []() -> std::string {
        return QCoreApplication::applicationName().toStdString();
    });

    app.set_function("version", []() -> std::string {
        return QCoreApplication::applicationVersion().toStdString();
    });

    app.set_function("organization", []() -> std::string {
        return QCoreApplication::organizationName().toStdString();
    });

    // Application paths
    app.set_function("application_dir", []() -> std::string {
        return QCoreApplication::applicationDirPath().toStdString();
    });

    app.set_function("working_dir", []() -> std::string {
        return QDir::currentPath().toStdString();
    });

    // Platform information
    app.set_function("os", []() -> std::string {
        return QSysInfo::productType().toStdString();
    });

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

    host["app"] = app;
}