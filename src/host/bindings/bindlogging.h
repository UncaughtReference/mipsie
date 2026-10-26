#ifndef BINDLOGGING_H
#define BINDLOGGING_H

#pragma once

#include <functional>
#include <QString>

namespace sol {
class state;
}

void bindLogging(sol::state& lua, const std::function<void(const QString &)> &output);

#endif // BINDLOGGING_H
