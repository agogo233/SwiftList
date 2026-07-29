#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

namespace swiftlist::pipe {

SECURITY_ATTRIBUTES* CreateEveryoneSecurity();

SECURITY_ATTRIBUTES* CreateCurrentUserOnlySecurity();

void FreeSecurity(SECURITY_ATTRIBUTES* sa);

} // namespace swiftlist::pipe
