#include "pipe/pipe_security.h"

namespace swiftlist::pipe {

SECURITY_ATTRIBUTES* CreateEveryoneSecurity() {
    // D:(A;;GA;;;WD)(A;;GA;;;AU)  — Everyone + Authenticated Users full control.
    constexpr wchar_t sddl[] =
        L"D:(A;;GA;;;WD)(A;;GA;;;AU)";

    auto* sa = static_cast<SECURITY_ATTRIBUTES*>(HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(SECURITY_ATTRIBUTES)));
    if (!sa) return nullptr;

    sa->nLength = sizeof(SECURITY_ATTRIBUTES);
    sa->bInheritHandle = FALSE;

    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            sddl, SDDL_REVISION_1, &sa->lpSecurityDescriptor, nullptr)) {
        HeapFree(GetProcessHeap(), 0, sa);
        return nullptr;
    }

    return sa;
}

SECURITY_ATTRIBUTES* CreateCurrentUserOnlySecurity() {
    // Only SYSTEM and the current user SID get full control.
    constexpr wchar_t sddl[] =
        L"D:(A;;GA;;;SY)(A;;GA;;;BA)(A;;GWGR;;;IU)";

    auto* sa = static_cast<SECURITY_ATTRIBUTES*>(HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(SECURITY_ATTRIBUTES)));
    if (!sa) return nullptr;

    sa->nLength = sizeof(SECURITY_ATTRIBUTES);
    sa->bInheritHandle = FALSE;

    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            sddl, SDDL_REVISION_1, &sa->lpSecurityDescriptor, nullptr)) {
        HeapFree(GetProcessHeap(), 0, sa);
        return nullptr;
    }

    return sa;
}

void FreeSecurity(SECURITY_ATTRIBUTES* sa) {
    if (!sa) return;
    if (sa->lpSecurityDescriptor) {
        LocalFree(sa->lpSecurityDescriptor);
    }
    HeapFree(GetProcessHeap(), 0, sa);
}

} // namespace swiftlist::pipe
