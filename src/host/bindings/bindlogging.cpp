#include "bindlogging.h"

#include <sol/sol.hpp>

#include <QString>

#include <string>

void bindLogging(sol::state& lua, const std::function<void(const QString &)> &output)
{
    sol::table host = lua["host"];

    // Create a table for the logging API.
    sol::table log = lua.create_table();

    log["info"] = [output](const std::string& message) {
        output(QStringLiteral("[INFO] %1").arg(QString::fromStdString(message)));
    };

    log["warn"] = [output](const std::string& message) {
        output(QStringLiteral("[WARN] %1").arg(QString::fromStdString(message)));
    };

    log["error"] = [output](const std::string& message) {
        output(QStringLiteral("[ERROR] %1").arg(QString::fromStdString(message)));
    };

    // Assign the TABLE, not a function, to host.log.
    host["log"] = log;
}
