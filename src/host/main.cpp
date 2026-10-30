#include "runtime/luaruntime.h"
#include "ui/mainwindow/mainwindow.h"

#include <QApplication>
#include <QColor>
#include <QCommandLineParser>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QImage>
#include <QIcon>
#include <QMainWindow>
#include <QPainter>
#include <QPoint>
#include <QPropertyAnimation>
#include <QSaveFile>
#include <QStandardPaths>
#include <QSize>
#include <QTimer>
#include <QVariantAnimation>
#include <QDebug>
#include <QSplashScreen>

#include <cmath>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace
{
class ScalableSplashScreen final : public QSplashScreen
{
public:
    explicit ScalableSplashScreen(const QPixmap &pixmap)
        : QSplashScreen(pixmap), sourcePixmap(pixmap)
    {
        if (!sourcePixmap.isNull()) {
            const QImage image = sourcePixmap.toImage();
            backgroundColor = image.pixelColor(image.width() / 2, image.height() / 10);
        }
    }

    void setScale(qreal scale)
    {
        imageScale = qBound<qreal>(0.0, scale, 1.0);
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.fillRect(rect(), backgroundColor);
        const QSize scaledSize(qRound(sourcePixmap.width() * imageScale),
                               qRound(sourcePixmap.height() * imageScale));
        const QRect targetRect(QPoint((width() - scaledSize.width()) / 2,
                                      (height() - scaledSize.height()) / 2),
                               scaledSize);
        painter.drawPixmap(targetRect, sourcePixmap);
    }

private:
    QPixmap sourcePixmap;
    QColor backgroundColor = Qt::transparent;
    qreal imageScale = 1.0;
};

QString initScriptPath()
{
    const QString localAppData = QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation);
    if (localAppData.isEmpty())
        return {};

    const QString path = QDir(localAppData).filePath(QStringLiteral("init.lua"));
    if (!QFileInfo::exists(path)) {
        QSaveFile file(path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.write("function init()\n"
                       "    print(\"Hello from init.lua!! <3\")\n"
                       "end\n");
            file.commit();
        }
    }
    return path;
}

#ifdef Q_OS_WIN
const QString jetBrainsMonoNerdFontFamily = QStringLiteral("JetBrainsMono Nerd Font");
const QString jetBrainsMonoNerdFontResource =
    QStringLiteral(":/mipsie/fonts/JetBrainsMonoNerdFont-Regular.ttf");

bool hasJetBrainsMonoNerdFont()
{
    const QStringList families = QFontDatabase().families();
    return families.contains(jetBrainsMonoNerdFontFamily, Qt::CaseInsensitive);
}

// Try to find the Fonts directory!!
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
#endif
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
#ifdef Q_OS_WIN
    QFont applicationFont = a.font();
    if (!hasJetBrainsMonoNerdFont() && !installJetBrainsMonoNerdFont())
        qWarning() << "JetBrainsMono Nerd Font is unavailable; using the system default font.";
    if (hasJetBrainsMonoNerdFont())
        applicationFont.setFamily(jetBrainsMonoNerdFontFamily);
    a.setFont(applicationFont);
#endif

    // Show a scaled splash and let Qt paint it before doing startup work.
    constexpr QSize splashMaxSize(350, 420);
    QPixmap splashImage(QStringLiteral(":/mipsie/splash.png"));
    if (!splashImage.isNull()) {
        splashImage = splashImage.scaled(
            splashMaxSize,
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation);
    } else {
        qWarning() << "Could not load the splash image resource.";
        splashImage = QPixmap(QStringLiteral(":/mipsie/logo.png"));
        if (!splashImage.isNull()) {
            splashImage = splashImage.scaled(
                splashMaxSize,
                Qt::KeepAspectRatio,
                Qt::SmoothTransformation);
        }
    }

    ScalableSplashScreen splash(splashImage);
    splash.setWindowOpacity(0.0);
    splash.show();

    auto *fadeIn = new QPropertyAnimation(&splash, "windowOpacity", &splash);
    fadeIn->setDuration(300);
    fadeIn->setStartValue(0.0);
    fadeIn->setEndValue(1.0);

    auto *scalePulse = new QVariantAnimation(&splash);
    scalePulse->setDuration(300);
    scalePulse->setStartValue(0.0);
    scalePulse->setEndValue(1.0);
    QObject::connect(scalePulse, &QVariantAnimation::valueChanged,
                     &splash, [&splash](const QVariant &value) {
        // Enough precision for ya?
        constexpr qreal pi = 3.14159265358979323846;
        const qreal phase = value.toReal() * (pi / 2.0);
        splash.setScale(std::sin(phase));
    });

    QEventLoop fadeLoop;
    QObject::connect(fadeIn, &QPropertyAnimation::finished,
                     &fadeLoop, &QEventLoop::quit);
    QObject::connect(scalePulse, &QVariantAnimation::finished,
                     &fadeLoop, &QEventLoop::quit);
    fadeIn->start();
    scalePulse->start();
    fadeLoop.exec();
    splash.setScale(1.0);

    MainWindow mainWindow;

    LuaRuntime runtime;
    const QString luaInitPath = initScriptPath();

    // Lua stuff
    QObject::connect(&runtime, &LuaRuntime::scriptError,
                     &mainWindow, &MainWindow::appendLuaError);
    QObject::connect(&runtime, &LuaRuntime::scriptOutput,
                     &mainWindow, &MainWindow::appendLuaOutput);
    QObject::connect(&mainWindow, &MainWindow::luaFunctionRequested,
                     &runtime, [&runtime, luaInitPath](const QString &functionName,
                                                       const QString &arguments) {
        if (luaInitPath.isEmpty() || !runtime.executeFile(luaInitPath))
            return;
        const QString invocation = QStringLiteral(
            "if type(%1) ~= \"function\" then error(\"No global function named '%1'\") end\n"
            "%1(%2)")
                                       .arg(functionName, arguments);
        runtime.executeString(invocation, QStringLiteral("=Lua Console"));
    });

    runtime.initialize(&mainWindow);

    if (!luaInitPath.isEmpty() && runtime.executeFile(luaInitPath)) {
        runtime.executeString(
            QStringLiteral("if type(init) == \"function\" then init() end"),
            QStringLiteral("=startup init"));
    }

// Idk why but showing maximized sometimes crashes on linux
#ifdef Q_OS_LINUX
    mainWindow.show();
#else
    mainWindow.showMaximized();
#endif
    splash.finish(&mainWindow);

    // Offer session recovery on every launch, then open any paths passed by the OS/CLI.
    QTimer::singleShot(0, &mainWindow, [&mainWindow, commandLinePaths] {
        mainWindow.offerSessionRecovery();
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
