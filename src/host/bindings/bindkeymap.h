#ifndef BINDKEYMAP_H
#define BINDKEYMAP_H

#include <sol/sol.hpp>
#include <functional>

#include <QString>

class QMainWindow;

void bindKeymap(sol::state &lua,
                QMainWindow *mainWindow,
                const std::function<void(const QString &)> &output);

#endif // BINDKEYMAP_H
