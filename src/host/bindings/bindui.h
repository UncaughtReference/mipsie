#ifndef BINDUI_H
#define BINDUI_H

#include <QMainWindow>

namespace sol {
class state;
}

void bindUI(sol::state &lua, QMainWindow *mainWindow);

#endif // BINDUI_H
