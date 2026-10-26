#include "runtime/luaruntime.h"
#include "ui/mainwindow/mainwindow.h"

#include <QApplication>
#include <QFont>
#include <QMainWindow>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QCoreApplication::setApplicationName("mipsie");
    QCoreApplication::setApplicationVersion("0.0.0");
    QCoreApplication::setOrganizationName("mipsie");

    a.setStyle("Fusion");
    QFont applicationFont = a.font();
    applicationFont.setFamily(QStringLiteral("JetBrainsMono Nerd Font"));
    a.setFont(applicationFont);

    MainWindow mainWindow;

    LuaRuntime runtime;

    QObject::connect(&runtime, &LuaRuntime::scriptError,
                     &mainWindow, &MainWindow::appendLuaError);
    QObject::connect(&runtime, &LuaRuntime::scriptOutput,
                     &mainWindow, &MainWindow::appendLuaOutput);

    runtime.initialize(&mainWindow);

    runtime.executeFile("lua/init.lua");

    mainWindow.show();

    const int exitCode = a.exec();

    runtime.shutdown();
    return exitCode;
}
