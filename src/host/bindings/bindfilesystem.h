#ifndef BINDFILESYSTEM_H
#define BINDFILESYSTEM_H

#pragma once

namespace sol {
class state;
}

class QMainWindow;

void bindFilesystem(sol::state &lua, QMainWindow *mainWindow);

#endif // BINDFILESYSTEM_H
