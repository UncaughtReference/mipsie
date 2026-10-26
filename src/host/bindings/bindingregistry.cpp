#include "bindingregistry.h"

#include "bindapplication.h"
#include "bindfilesystem.h"
#include "bindlogging.h"
#include "bindui.h"

void BindingRegistry::registerAll(sol::state &lua, QMainWindow *mainWindow,
                                  const std::function<void(const QString &)> &output)
{
    lua["host"] = lua.create_table();

    bindLogging(lua, output);
    bindApplication(lua);
    bindFilesystem(lua);
    bindUi(lua, mainWindow);
}
