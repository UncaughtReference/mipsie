#ifndef BINDUI_H
#define BINDUI_H

#include "qmainwindow.h"
#pragma once

namespace sol {
class state;
}

void bindUi(sol::state& lua, QMainWindow* mainWindow);

#endif // BINDUI_H
