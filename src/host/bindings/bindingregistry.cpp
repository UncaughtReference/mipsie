#include "bindingregistry.h"

#include "bindapplication.h"
#include "bindfilesystem.h"
#include "bindlogging.h"
#include "bindui.h"
#include "bindhttp.h"

void BindingRegistry::registerAll(sol::state &lua, QMainWindow *mainWindow,
                                  QNetworkAccessManager *networkManager,
                                  const std::function<void(const QString &)> &output)
{
    lua["mipsie"] = lua.create_table();

    bindLogging(lua, output);
    bindApplication(lua);
    bindFilesystem(lua);
    bindUI(lua, mainWindow);
    bindHttp(lua, networkManager, output);
}
