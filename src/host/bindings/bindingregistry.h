#ifndef BINDINGREGISTRY_H
#define BINDINGREGISTRY_H

#include "qmainwindow.h"
#pragma once

#include <sol/sol.hpp>
#include <functional>
#include <QString>

class BindingRegistry
{
public:
    static void registerAll(sol::state &lua, QMainWindow* mainWindow,
                            const std::function<void(const QString &)> &output);
};

#endif // BINDINGREGISTRY_H
