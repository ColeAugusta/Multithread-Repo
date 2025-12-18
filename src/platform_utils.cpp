#include "../include/platform_wrapper.h"
#include <cstring>

// Initialize for platform wrapper
// handles socket creation and cleanup both platforms

namespace PlatformUtils {

bool initialize() {
    return Socket::initializeSockets();
}

void cleanup() {
    Socket::cleanupSockets();
}

std::string getLastErrorString() {
#ifdef _WIN32
    DWORD errorCode = GetLastError();
    char* msgBuf = nullptr;
    
    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
        nullptr,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&msgBuf,
        0,
        nullptr
    );
    
    std::string errorMsg = msgBuf ? msgBuf : "Unknown error";
    if (msgBuf) LocalFree(msgBuf);
    
    return errorMsg;
#else
    return std::string(strerror(errno));
#endif
}

}