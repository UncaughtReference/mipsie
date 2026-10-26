#ifndef PATHRESOLVER_H
#define PATHRESOLVER_H


#pragma once

#include <QCoreApplication>
#include <QDir>
#include <QString>

namespace PathResolver
{
// Base directory for resolving relative paths.
// Debug: current working directory (configure to project root).
// Release: directory containing the executable.
inline QString baseDirectory()
{
#ifdef MIPSIE_DEV_BUILD
    return QDir::currentPath();
#else
    return QCoreApplication::applicationDirPath();
#endif
}

// Resolve a path against the configured base directory.
// Absolute paths remain absolute.
inline QString resolve(const QString& path)
{
    QDir base(baseDirectory());
    return base.absoluteFilePath(path);
}

// Directory containing the executable.
inline QString applicationDirectory()
{
    return QCoreApplication::applicationDirPath();
}

// Process's current working directory.
inline QString workingDirectory()
{
    return QDir::currentPath();
}

// Current project root.
// Until project management exists, use the development working
// directory in Debug and the executable directory in Release.
inline QString projectDirectory()
{
#ifdef MIPSIE_DEV_BUILD
    return QDir::currentPath();
#else
    return QCoreApplication::applicationDirPath();
#endif
}
}

#endif // PATHRESOLVER_H
