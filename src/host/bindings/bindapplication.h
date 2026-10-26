#ifndef BINDAPPLICATION_H
#define BINDAPPLICATION_H

#pragma once

namespace sol {
class state;
}

void bindApplication(sol::state& lua);

#endif // BINDAPPLICATION_H
