#ifndef BINDFILESYSTEM_H
#define BINDFILESYSTEM_H

#pragma once

namespace sol {
class state;
}

void bindFilesystem(sol::state& lua);

#endif // BINDFILESYSTEM_H
