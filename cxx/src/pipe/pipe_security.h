#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace swiftlist::pipe {

SECURITY_ATTRIBUTES* CreateEveryoneSecurity();

SECURITY_ATTRIBUTES* CreateCurrentUserOnlySecurity();

void FreeSecurity(SECURITY_ATTRIBUTES* sa);

} // namespace swiftlist::pipe
