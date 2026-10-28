#include "bindfilesystem.h"

#include "pathresolver.h"
#include "../ui/mainwindow/mainwindow.h"

#include <sol/sol.hpp>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QTextStream>

#include <stdexcept>
#include <string>

namespace
{
QString toQString(const std::string& value)
{
    return QString::fromUtf8(value.data(),
                             static_cast<qsizetype>(value.size()));
}

QString resolvePath(const std::string& path)
{
    return PathResolver::resolve(toQString(path));
}

std::string toStdString(const QString& value)
{
    const QByteArray utf8 = value.toUtf8();
    return std::string(utf8.constData(),
                       static_cast<std::size_t>(utf8.size()));
}
}

void bindFilesystem(sol::state &lua, QMainWindow *mainWindow)
{
    sol::table mipsie = lua["mipsie"];
    sol::table fs = lua.create_table();
    const QPointer<MainWindow> guardedWindow(qobject_cast<MainWindow *>(mainWindow));

    // Check whether a file or directory exists.
    fs.set_function("exists", [](const std::string& path) {
        return QFileInfo::exists(resolvePath(path));
    });

    // Read a text file. Throws on failure.
    fs.set_function("read_text", [](const std::string& path) {
        const QString resolved = resolvePath(path);
        QFile file(resolved);

        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            throw std::runtime_error(
                toStdString(
                    QStringLiteral("Cannot read '%1': %2")
                        .arg(resolved, file.errorString())
                    )
                );
        }

        return toStdString(QString::fromUtf8(file.readAll()));
    });

    // Write a text file. Parent directories must already exist.
    fs.set_function("write_text",
                    [](const std::string& path, const std::string& contents) {
                        const QString resolved = resolvePath(path);
                        QFile file(resolved);

                        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                            throw std::runtime_error(
                                toStdString(
                                    QStringLiteral("Cannot write '%1': %2")
                                        .arg(resolved, file.errorString())
                                    )
                                );
                        }

                        const QByteArray data = QByteArray::fromStdString(contents);
                        const qint64 written = file.write(data);

                        if (written != data.size()) {
                            throw std::runtime_error(
                                toStdString(
                                    QStringLiteral("Failed writing '%1': %2")
                                        .arg(resolved, file.errorString())
                                    )
                                );
                        }
                    }
                    );

    // Return file and directory names as a Lua array.

    fs.set_function("list_dir", [&lua](const std::string& path) {
        const QString resolved = resolvePath(path);
        QDir dir(resolved);

        if (!dir.exists()) {
            throw std::runtime_error(
                toStdString(
                    QStringLiteral("Directory does not exist: %1")
                        .arg(resolved)
                    )
                );
        }

        sol::table result = lua.create_table();
        const QFileInfoList entries = dir.entryInfoList(
            QDir::NoDotAndDotDot | QDir::AllEntries,
            QDir::Name
            );

        int index = 1;
        for (const QFileInfo& entry : entries) {
            result[index++] = toStdString(entry.fileName());
        }

        return result;
    });


    fs.set_function("app_dir", []() {
        return toStdString(PathResolver::applicationDirectory());
    });

    fs.set_function("project_dir", []() {
        return toStdString(PathResolver::projectDirectory());
    });

    fs.set_function("rom_path", [guardedWindow]() {
        return toStdString(guardedWindow ? guardedWindow->currentRomFilePath() : QString());
    });

    fs.set_function("current_project_dir", [guardedWindow]() {
        return toStdString(guardedWindow ? guardedWindow->currentProjectDirectory() : QString());
    });

    fs.set_function("base_dir", []() {
        return toStdString(PathResolver::baseDirectory());
    });

    fs.set_function("current_dir", []() {
        return toStdString(PathResolver::workingDirectory());
    });

    fs.set_function("join",
                    [](const std::string& first, const std::string& second) {
                        return toStdString(
                            QDir(toQString(first)).filePath(toQString(second))
                            );
                    }
                    );

    mipsie["fs"] = fs;
}

