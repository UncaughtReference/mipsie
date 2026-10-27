#include "runtime/luaruntime.h"
#include "ui/mainwindow/mainwindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QIcon>
#include <QMainWindow>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>
#include <QDebug>

#include <windows.h>

namespace
{
const QString jetBrainsMonoNerdFontFamily = QStringLiteral("JetBrainsMono Nerd Font");
const QString jetBrainsMonoNerdFontResource =
    QStringLiteral(":/mipsie/fonts/JetBrainsMonoNerdFont-Regular.ttf");

bool hasJetBrainsMonoNerdFont()
{
    const QStringList families = QFontDatabase().families();
    return families.contains(jetBrainsMonoNerdFontFamily, Qt::CaseInsensitive);
}

QString userFontDirectory()
{
    const QString localAppData = qEnvironmentVariable("LOCALAPPDATA");
    if (!localAppData.isEmpty())
        return QDir(localAppData).filePath(QStringLiteral("Microsoft/Windows/Fonts"));
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
        .filePath(QStringLiteral("Fonts"));
}

bool registerWindowsUserFont(const QString &fontPath)
{
    HKEY fontsKey = nullptr;
    const LONG openResult = RegCreateKeyExW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Fonts",
        0, nullptr, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &fontsKey, nullptr);
    if (openResult != ERROR_SUCCESS)
        return false;

    const std::wstring nativeFontPath = QDir::toNativeSeparators(fontPath).toStdWString();
    const wchar_t valueName[] = L"JetBrainsMono Nerd Font Regular (TrueType)";
    const LONG writeResult = RegSetValueExW(
        fontsKey, valueName, 0, REG_SZ,
        reinterpret_cast<const BYTE *>(nativeFontPath.c_str()),
        static_cast<DWORD>((nativeFontPath.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(fontsKey);
    if (writeResult != ERROR_SUCCESS)
        return false;

    DWORD_PTR result = 0;
    SendMessageTimeoutW(HWND_BROADCAST, WM_FONTCHANGE, 0, 0,
                        SMTO_ABORTIFHUNG, 2000, &result);
    return true;
}

bool installJetBrainsMonoNerdFont()
{
    if (hasJetBrainsMonoNerdFont())
        return true;

    const QString fontDirectory = userFontDirectory();
    if (fontDirectory.isEmpty() || !QDir().mkpath(fontDirectory))
        return false;

    const QString fontPath = QDir(fontDirectory).filePath(
        QStringLiteral("JetBrainsMonoNerdFont-Regular.ttf"));
    if (!QFileInfo::exists(fontPath)) {
        QFile fontResource(jetBrainsMonoNerdFontResource);
        if (!fontResource.open(QIODevice::ReadOnly))
            return false;
        QSaveFile fontFile(fontPath);
        if (!fontFile.open(QIODevice::WriteOnly)
            || fontFile.write(fontResource.readAll()) < 0
            || !fontFile.commit())
            return false;
    }

    QFile licenseResource(QStringLiteral(":/mipsie/fonts/JetBrainsMonoNerdFont-OFL.txt"));
    QFile licenseFile(QDir(fontDirectory).filePath(
        QStringLiteral("JetBrainsMonoNerdFont-OFL.txt")));
    if (licenseResource.open(QIODevice::ReadOnly) && licenseFile.open(QIODevice::WriteOnly))
        licenseFile.write(licenseResource.readAll());

    if (!registerWindowsUserFont(fontPath))
        qWarning() << "Could not register the font in the current user's Windows font list.";

    return QFontDatabase::addApplicationFont(fontPath) >= 0;
}
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // App info!!
    QCoreApplication::setApplicationName("mipsie");
    QCoreApplication::setApplicationVersion("1.0.0");
    QCoreApplication::setOrganizationName("mipsie");

    // Set up the CLI
    QCommandLineParser parser;
    parser.setApplicationDescription(QObject::tr("Mipsie SM64 Assembly IDE"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(
        QStringLiteral("path"),
        QObject::tr("Open an assembly file or project folder."),
        QStringLiteral("[path...]")
        );
    parser.process(a);
    const QStringList commandLinePaths = parser.positionalArguments();

    // Not sure if this is needed but idc
    a.setWindowIcon(QIcon(QStringLiteral(":/mipsie/logo.png")));

    // Set initial style and font
    a.setStyle("Fusion");
    QFont applicationFont = a.font();
    if (!hasJetBrainsMonoNerdFont() && !installJetBrainsMonoNerdFont())
        qWarning() << "JetBrainsMono Nerd Font is unavailable; using the system default font.";
    if (hasJetBrainsMonoNerdFont())
        applicationFont.setFamily(jetBrainsMonoNerdFontFamily);
    a.setFont(applicationFont);

    MainWindow mainWindow;

    LuaRuntime runtime;

    // Lua stuff
    QObject::connect(&runtime, &LuaRuntime::scriptError,
                     &mainWindow, &MainWindow::appendLuaError);
    QObject::connect(&runtime, &LuaRuntime::scriptOutput,
                     &mainWindow, &MainWindow::appendLuaOutput);
    QObject::connect(&mainWindow, &MainWindow::luaFunctionRequested,
                     &runtime, [&runtime](const QString &functionName,
                                          const QString &arguments) {
        if (!runtime.executeFile(QStringLiteral("lua/init.lua")))
            return;
        const QString invocation = QStringLiteral(
            "if type(%1) ~= \"function\" then error(\"No global function named '%1'\") end\n"
            "%1(%2)")
                                       .arg(functionName, arguments);
        runtime.executeString(invocation, QStringLiteral("=Lua Console"));
    });

    runtime.initialize(&mainWindow);

    if (runtime.executeFile(QStringLiteral("lua/init.lua"))) {
        runtime.executeString(
            QStringLiteral("if type(init) == \"function\" then init() end"),
            QStringLiteral("=startup init"));
    }

    mainWindow.showMaximized();

    // Finish startup path loading before returning focus to the editor.
    QTimer::singleShot(0, &mainWindow, [&mainWindow, commandLinePaths] {
        if (!commandLinePaths.isEmpty()) {
            for (const QString &path : commandLinePaths)
                mainWindow.openCommandLinePath(path);
        }
        mainWindow.focusEditor();
    });

    const int exitCode = a.exec();

    // Shut down the Lua runtime
    runtime.shutdown();
    return exitCode;
}
