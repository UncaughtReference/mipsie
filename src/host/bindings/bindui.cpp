
#include "bindui.h"
#include "qmainwindow.h"
#include "qmenubar.h"

#include <sol/sol.hpp>

#include <QApplication>
#include <QByteArray>
#include <QBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QMetaObject>
#include <QObject>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QThread>
#include <QVBoxLayout>
#include <QWidget>
#include <QDebug>

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{
class UiMenu;
class UiMenuBar;

// ------------------------------------------------------------
// Utilities
// ------------------------------------------------------------

void requireGuiThread()
{
    if (!qApp || QThread::currentThread() != qApp->thread()) {
        throw std::runtime_error(
            "UI operations must run on the GUI thread"
            );
    }
}

std::string toStdString(const QString& value)
{
    const QByteArray utf8 = value.toUtf8();

    return std::string(
        utf8.constData(),
        static_cast<std::size_t>(utf8.size())
        );
}

QString toQString(const std::string& value)
{
    return QString::fromUtf8(
        value.data(),
        static_cast<int>(value.size())
        );
}

std::string getStringOption(
    sol::table options,
    const char* key,
    const std::string& fallback = {})
{
    sol::optional<std::string> value =
        options.get<sol::optional<std::string>>(key);

    return value.value_or(fallback);
}

int getIntOption(
    sol::table options,
    const char* key,
    int fallback)
{
    sol::optional<int> value =
        options.get<sol::optional<int>>(key);

    return value.value_or(fallback);
}

// ------------------------------------------------------------
// Widget wrapper
// ------------------------------------------------------------

class UiWidget
{
public:
    explicit UiWidget(QWidget* widget)
        : widget_(widget)
    {
        if (!widget)
            throw std::runtime_error("Cannot wrap a null widget");
    }

    ~UiWidget()
    {
        clearCallback();

        // Qt may already have destroyed this widget through
        // its parent. QPointer becomes null in that case.
        if (widget_)
            delete widget_.data();
    }

    QWidget* get() const
    {
        if (!widget_)
            throw std::runtime_error("Widget no longer exists");

        return widget_.data();
    }

    void setText(const std::string& text)
    {
        requireGuiThread();

        QWidget* widget = get();
        const QString value = toQString(text);

        if (auto* label = qobject_cast<QLabel*>(widget)) {
            label->setText(value);
            return;
        }

        if (auto* button = qobject_cast<QPushButton*>(widget)) {
            button->setText(value);
            return;
        }

        if (auto* editor =
            qobject_cast<QPlainTextEdit*>(widget)) {
            editor->setPlainText(value);
            return;
        }

        throw std::runtime_error(
            "set_text() is not supported by this widget"
            );
    }

    std::string text() const
    {
        requireGuiThread();

        QWidget* widget = get();

        if (auto* label = qobject_cast<QLabel*>(widget))
            return toStdString(label->text());

        if (auto* button = qobject_cast<QPushButton*>(widget))
            return toStdString(button->text());

        if (auto* editor =
            qobject_cast<QPlainTextEdit*>(widget)) {
            return toStdString(editor->toPlainText());
        }

        return {};
    }

    void setPlaceholder(const std::string& text)
    {
        requireGuiThread();

        auto* editor =
            qobject_cast<QPlainTextEdit*>(get());

        if (!editor) {
            throw std::runtime_error(
                "set_placeholder() requires a text editor"
                );
        }

        editor->setPlaceholderText(toQString(text));
    }

    void onClick(sol::protected_function callback)
    {
        requireGuiThread();

        auto* button =
            qobject_cast<QPushButton*>(get());

        if (!button) {
            throw std::runtime_error(
                "on_click() requires a button"
                );
        }

        // Replace any previously registered callback.
        clearCallback();

        callback_ = std::move(callback);

        clickConnection_ = QObject::connect(
            button,
            &QPushButton::clicked,
            button,
            [this]() {
                if (!callback_.valid())
                    return;

                sol::protected_function_result result =
                    callback_();

                if (!result.valid()) {
                    sol::error error = result;

                    qWarning().noquote()
                        << "Lua button callback failed:"
                        << QString::fromUtf8(error.what());
                }
            }
            );
    }

    void clearCallback()
    {
        QObject::disconnect(clickConnection_);
        clickConnection_ = QMetaObject::Connection{};

        // Release the Lua function while the Lua state is alive.
        callback_ = sol::protected_function{};
    }

private:
    QPointer<QWidget> widget_;
    sol::protected_function callback_;
    QMetaObject::Connection clickConnection_;
};

// ------------------------------------------------------------
// Layout wrapper
// ------------------------------------------------------------

class UiLayout
{
public:
    explicit UiLayout(QLayout* layout)
        : layout_(layout)
    {
        if (!layout)
            throw std::runtime_error("Cannot wrap a null layout");
    }

    ~UiLayout()
    {
        // A window may already have deleted this layout.
        if (layout_)
            delete layout_.data();
    }

    QLayout* get() const
    {
        if (!layout_)
            throw std::runtime_error("Layout no longer exists");

        return layout_.data();
    }

    void add(const std::shared_ptr<UiWidget>& widget)
    {
        requireGuiThread();

        if (!widget)
            throw std::runtime_error("Cannot add a null widget");

        auto* box = qobject_cast<QBoxLayout*>(get());

        if (!box) {
            throw std::runtime_error(
                "Unsupported layout type"
                );
        }

        box->addWidget(widget->get());

        // Keep the wrapper alive while the layout uses the widget.
        widgets_.push_back(widget);
    }

private:
    QPointer<QLayout> layout_;
    std::vector<std::shared_ptr<UiWidget>> widgets_;
};

// ------------------------------------------------------------
// Window wrapper
// ------------------------------------------------------------

class UiWindow
{
public:
    explicit UiWindow(const sol::table& options)
    {
        requireGuiThread();

        window = new QMainWindow();

        const std::string title =
            getStringOption(options, "title", "Mipsie");

        const int width =
            getIntOption(options, "width", 1100);

        const int height =
            getIntOption(options, "height", 750);

        window->setWindowTitle(toQString(title));
        window->resize(width, height);

        // Create the central content widget.
        auto* centralWidget = new QWidget(window);
        auto* layout = new QVBoxLayout(centralWidget);

        centralWidget->setLayout(layout);
        window->setCentralWidget(centralWidget);
    }

    ~UiWindow()
    {
        if (window) {
            delete window;
        }
    }

    QWidget* get() const
    {
        return window.data();
    }

    QMainWindow* getMainWindow() const
    {
        return window.data();
    }

    void setTitle(const std::string& title)
    {
        requireGuiThread();

        if (window) {
            window->setWindowTitle(toQString(title));
        }
    }

    void setSize(int width, int height)
    {
        requireGuiThread();

        if (window) {
            window->resize(width, height);
        }
    }

    void setLayout(const std::shared_ptr<UiLayout>& layout)
    {
        requireGuiThread();

        if (!window || !layout || !layout->get()) {
            return;
        }

        QWidget* central = window->centralWidget();

        // Create a central widget if this is the first layout.
        if (!central) {
            central = new QWidget(window);
            window->setCentralWidget(central);
        }

        QLayout* newLayout = layout->get();

        // Don't replace the layout if it's already installed.
        if (central->layout() == newLayout) {
            return;
        }

        // A layout cannot be installed on two widgets.
        if (newLayout->parent() && newLayout->parent() != central) {
            newLayout->setParent(nullptr);
        }

        // Remove the existing layout, if any.
        if (central->layout()) {
            delete central->layout();
        }

        central->setLayout(newLayout);
    }

    void show()
    {
        requireGuiThread();

        if (window) {
            window->show();
        }
    }

    void showMaximized()
    {
        requireGuiThread();

        if (window) {
            window->showMaximized();
        }
    }

    void close()
    {
        requireGuiThread();

        if (window) {
            window->close();
        }
    }

    std::shared_ptr<UiMenuBar> menuBar();

private:
    QPointer<QMainWindow> window;
};

std::shared_ptr<UiMenuBar> UiWindow::menuBar()
{
    requireGuiThread();

    if (!window) {
        return nullptr;
    }

    QMenuBar* bar = window->menuBar();

    return std::make_shared<UiMenuBar>(bar);
}

class UiMenuBar
{
public:
    explicit UiMenuBar(QMenuBar* bar)
        : menuBar(bar)
    {
    }

    QMenuBar* get() const
    {
        return menuBar.data();
    }

    std::shared_ptr<UiMenu> addMenu(const std::string& title)
    {
        requireGuiThread();

        if (!menuBar) {
            return nullptr;
        }

        QMenu* menu = menuBar->addMenu(toQString(title));

        auto wrapper = std::make_shared<UiMenu>(menu);
        menus.push_back(wrapper);

        return wrapper;
    }

private:
    QPointer<QMenuBar> menuBar;
    std::vector<std::shared_ptr<UiMenu>> menus;
};

class UiAction
{
public:
    explicit UiAction(const sol::table& options)
    {
        requireGuiThread();

        action = new QAction();

        const std::string text = getStringOption(options, "text", "");
        const std::string shortcut = getStringOption(options, "shortcut", "");
        const std::string statusTip = getStringOption(options, "status_tip", "");

        action->setText(toQString(text));

        if (!shortcut.empty()) {
            action->setShortcut(
                QKeySequence(toQString(shortcut))
                );
        }

        action->setStatusTip(toQString(statusTip));

        QObject::connect(
            action,
            &QAction::triggered,
            action,
            [this](bool checked) {
                if (!callback.valid()) {
                    return;
                }

                sol::protected_function_result result = callback(checked);

                if (!result.valid()) {
                    sol::error error = result;
                    qWarning().noquote()
                        << "Lua action callback failed:"
                        << QString::fromUtf8(error.what());
                }
            }
            );
    }

    ~UiAction()
    {
        clearCallback();

        if (action) {
            delete action;
        }
    }

    QAction* get() const
    {
        return action.data();
    }

    void setText(const std::string& text)
    {
        requireGuiThread();

        if (action) {
            action->setText(toQString(text));
        }
    }

    void setShortcut(const std::string& shortcut)
    {
        requireGuiThread();

        if (action) {
            action->setShortcut(
                QKeySequence(toQString(shortcut))
                );
        }
    }

    void setEnabled(bool enabled)
    {
        requireGuiThread();

        if (action) {
            action->setEnabled(enabled);
        }
    }

    void setCheckable(bool checkable)
    {
        requireGuiThread();

        if (action) {
            action->setCheckable(checkable);
        }
    }

    bool isChecked() const
    {
        return action && action->isChecked();
    }

    void setChecked(bool checked)
    {
        requireGuiThread();

        if (action) {
            action->setChecked(checked);
        }
    }

    void onTriggered(sol::protected_function fn)
    {
        requireGuiThread();
        callback = std::move(fn);
    }

    void clearCallback()
    {
        callback = sol::protected_function{};
    }

private:
    QPointer<QAction> action;
    sol::protected_function callback;
};

class UiMenu
{
public:
    explicit UiMenu(const std::string& title)
    {
        requireGuiThread();
        menu = new QMenu(toQString(title));
    }

    explicit UiMenu(QMenu* existingMenu)
        : menu(existingMenu)
    {
    }

    ~UiMenu()
    {
        if (menu) {
            delete menu;
        }
    }

    QMenu* get() const
    {
        return menu.data();
    }

    void addAction(const std::shared_ptr<UiAction>& action)
    {
        requireGuiThread();

        if (!menu || !action || !action->get()) {
            return;
        }

        menu->addAction(action->get());

        // Keep the Lua-side wrapper alive while the menu uses it.
        actions.push_back(action);
    }

    void show()
    {
        requireGuiThread();

        if (menu) {
            menu->exec(QCursor::pos());
        }
    }

private:
    QPointer<QMenu> menu;
    std::vector<std::shared_ptr<UiAction>> actions;
};

// ------------------------------------------------------------
// Wrapper tracking for shutdown
// ------------------------------------------------------------

std::vector<std::weak_ptr<UiWidget>> allWidgets;
std::vector<std::weak_ptr<UiWindow>> allWindows;
std::vector<std::weak_ptr<UiAction>> allActions;

void shutdownUi()
{
    requireGuiThread();

    // Disconnect callbacks and release their Lua references first.
    for (auto& weakWidget : allWidgets) {
        if (auto widget = weakWidget.lock())
            widget->clearCallback();
    }

    // Close windows while Qt and the Lua state still exist.
    for (auto& weakWindow : allWindows) {
        if (auto window = weakWindow.lock())
            window->close();
    }

    for (auto& weak : allActions) {
        if (auto action = weak.lock()) {
            action->clearCallback();
        }
    }

    allWidgets.clear();
    allWindows.clear();
    allActions.clear();
}

} // namespace

void bindUi(sol::state& lua, QMainWindow* mainWindow)
{
    sol::table host = lua["host"];
    sol::table ui = lua.create_table();

    // Register wrapper types before exposing their constructors.
    lua.new_usertype<UiWidget>(
        "UiWidget",
        "set_text", &UiWidget::setText,
        "text", &UiWidget::text,
        "set_placeholder", &UiWidget::setPlaceholder,
        "on_click", &UiWidget::onClick
        );

    lua.new_usertype<UiLayout>(
        "UiLayout",
        "add", &UiLayout::add
        );

    lua.new_usertype<UiWindow>(
        "UiWindow",
        "set_title", &UiWindow::setTitle,
        "set_size", &UiWindow::setSize,
        "set_layout", &UiWindow::setLayout,
        "show", &UiWindow::show,
        "show_maximized", &UiWindow::showMaximized,
        "menu_bar", &UiWindow::menuBar,
        "close", &UiWindow::close
        );

    lua.new_usertype<UiAction>(
        "UiAction",
        "set_text", &UiAction::setText,
        "set_shortcut", &UiAction::setShortcut,
        "set_enabled", &UiAction::setEnabled,
        "set_checkable", &UiAction::setCheckable,
        "is_checked", &UiAction::isChecked,
        "set_checked", &UiAction::setChecked,
        "on_triggered", &UiAction::onTriggered,
        "clear_callback", &UiAction::clearCallback
        );

    lua.new_usertype<UiMenu>(
        "UiMenu",
        "add_action", &UiMenu::addAction,
        "show", &UiMenu::show
        );

    lua.new_usertype<UiMenuBar>(
        "UiMenuBar",
        "add_menu", &UiMenuBar::addMenu
        );

    // Create a window.
    ui.set_function("window", [](const sol::table& options) {
        requireGuiThread();

        auto result = std::make_shared<UiWindow>(options);
        allWindows.push_back(result);

        return result;
    });

    QPointer<QMainWindow> window = mainWindow;

    ui.set_function("show_main_window", [window]() {
        if (window) {
            window->show();
        }
    });

    ui.set_function("maximize_main_window", [window]() {
        if (window) {
            window->showMaximized();
        }
    });

    // Create layouts.
    ui.set_function("vertical_layout", []() {
        requireGuiThread();

        return std::make_shared<UiLayout>(
            new QVBoxLayout()
            );
    });

    ui.set_function("horizontal_layout", []() {
        requireGuiThread();

        return std::make_shared<UiLayout>(
            new QHBoxLayout()
            );
    });

    // Create a label.
    ui.set_function("label", [](sol::table options) {
        requireGuiThread();

        auto* label = new QLabel(
            toQString(getStringOption(options, "text"))
            );

        auto result = std::make_shared<UiWidget>(label);
        allWidgets.push_back(result);

        return result;
    });

    // Create a button.
    ui.set_function("button", [](sol::table options) {
        requireGuiThread();

        auto* button = new QPushButton(
            toQString(
                getStringOption(options, "text", "Button")
                )
            );

        auto result = std::make_shared<UiWidget>(button);
        allWidgets.push_back(result);

        return result;
    });

    // Create a plain-text editor.
    ui.set_function("text_editor", [](sol::table options) {
        requireGuiThread();

        auto* editor = new QPlainTextEdit();

        editor->setPlaceholderText(
            toQString(getStringOption(options, "placeholder"))
            );

        editor->setPlainText(
            toQString(getStringOption(options, "text"))
            );

        auto result = std::make_shared<UiWidget>(editor);
        allWidgets.push_back(result);

        return result;
    });

    // Create an action
    ui.set_function("action", [](const sol::table& options) {
        requireGuiThread();

        return std::make_shared<UiAction>(options);
    });

    // Create a menu
    ui.set_function("menu", [](const sol::table& options) {
        requireGuiThread();

        const std::string title =
            getStringOption(options, "title", "");

        return std::make_shared<UiMenu>(title);
    });

    // Called from LuaRuntime::shutdown() before destroying Lua.
    ui.set_function("shutdown", []() {
        shutdownUi();
    });

    host["ui"] = ui;
}