#ifndef BINDHTTP_H
#define BINDHTTP_H

#include <functional>
#include <QString>

class QNetworkAccessManager;

namespace sol {
class state;
}

void bindHttp(sol::state &lua, QNetworkAccessManager *networkManager,
              const std::function<void(const QString &)> &output);

#endif // BINDHTTP_H
