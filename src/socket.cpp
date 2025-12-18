#include "../include/platform_wrapper.h"
#include <cstring>

bool Socket::s_initialized = false;

// Socket function implementations

Socket::Socket() : m_socket(INVALID_SOCKET_HANDLE), m_isValid(false) {
    initializeSockets();
}

Socket::Socket(SocketHandle handle) : m_socket(handle), m_isValid(handle != INVALID_SOCKET_HANDLE) {
    initializeSockets();
}

Socket::~Socket() {
    close();
}

Socket::Socket(Socket&& other) noexcept 
    : m_socket(other.m_socket), m_isValid(other.m_isValid) {
    other.m_socket = INVALID_SOCKET_HANDLE;
    other.m_isValid = false;
}

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        close();
        m_socket = other.m_socket;
        m_isValid = other.m_isValid;
        other.m_socket = INVALID_SOCKET_HANDLE;
        other.m_isValid = false;
    }
    return *this;
}


bool Socket::initializeSockets() {
    if (s_initialized) return true;
    
// need windows WSADATA from some reason
#ifdef _WIN32
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        return false;
    }
#endif
    
    s_initialized = true;
    return true;
}

// only need for WSAcleanup
void Socket::cleanupSockets() {
#ifdef _WIN32
    if (s_initialized) {
        WSACleanup();
        s_initialized = false;
    }
#endif
}


bool Socket::create() {
    if (m_isValid) {
        close();
    }
    
    m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    m_isValid = (m_socket != INVALID_SOCKET_HANDLE);
    
    return m_isValid;
}


bool Socket::bind(uint16_t port, const std::string& address) {
    if (!m_isValid) return false;
    
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    if (address == "0.0.0.0" || address.empty()) {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        addr.sin_addr.s_addr = inet_addr(address.c_str());
    }
    
    int result = ::bind(m_socket, (sockaddr*)&addr, sizeof(addr));
    return result == 0;
}


// listen writes to backlog
bool Socket::listen(int backlog) {
    if (!m_isValid) return false;
    
    int result = ::listen(m_socket, backlog);
    return result == 0;
}


Socket* Socket::accept() {
    if (!m_isValid) return nullptr;
    
    sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);
    
    SocketHandle clientSocket = ::accept(m_socket, (sockaddr*)&clientAddr, &clientAddrLen);
    
    if (clientSocket == INVALID_SOCKET_HANDLE) {
        return nullptr;
    }
    
    return new Socket(clientSocket);
}


bool Socket::connect(const std::string& host, uint16_t port) {
    if (!m_isValid) return false;
    
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    // tries to use IP address first
    addr.sin_addr.s_addr = inet_addr(host.c_str());
    
    // if not try DNS resolution
    if (addr.sin_addr.s_addr == INADDR_NONE) {
        struct hostent* he = gethostbyname(host.c_str());
        if (he == nullptr) {
            return false;
        }
        std::memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);
    }
    
    int result = ::connect(m_socket, (sockaddr*)&addr, sizeof(addr));
    return result == 0;
}


int Socket::send(const void* data, size_t length) {
    if (!m_isValid) return -1;

    
#ifdef _WIN32
    return ::send(m_socket, static_cast<const char*>(data), static_cast<int>(length), 0);
#else
    return ::send(m_socket, data, length, 0);
#endif
}

int Socket::receive(void* buffer, size_t length) {
    if (!m_isValid) return -1;
    
#ifdef _WIN32
    return ::recv(m_socket, static_cast<char*>(buffer), static_cast<int>(length), 0);
#else
    return ::recv(m_socket, buffer, length, 0);
#endif
}

bool Socket::setNonBlocking(bool nonBlocking) {
    if (!m_isValid) return false;
    
#ifdef _WIN32
    u_long mode = nonBlocking ? 1 : 0;
    return ioctlsocket(m_socket, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(m_socket, F_GETFL, 0);
    if (flags == -1) return false;
    
    if (nonBlocking) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }
    
    return fcntl(m_socket, F_SETFL, flags) == 0;
#endif
}

bool Socket::setReuseAddr(bool reuse) {
    if (!m_isValid) return false;
    
    int optval = reuse ? 1 : 0;
    int result = setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, 
#ifdef _WIN32
        (const char*)&optval,
#else
        &optval,
#endif
        sizeof(optval));
    
    return result == 0;
}

void Socket::close() {
    if (m_isValid) {
#ifdef _WIN32
        ::closesocket(m_socket);
#else
        ::close(m_socket);
#endif
        m_socket = INVALID_SOCKET_HANDLE;
        m_isValid = false;
    }
}

bool Socket::isValid() const {
    return m_isValid;
}

std::string Socket::getLastError() const {
#ifdef _WIN32
    // if on windows have to get error through WSA
    int errorCode = WSAGetLastError();
    char* msgBuf = nullptr;
    
    // not sure if this format works 
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

std::string Socket::getPeerAddress() const {
    if (!m_isValid) return "";
    
    sockaddr_in addr;
    socklen_t addrLen = sizeof(addr);
    
    if (getpeername(m_socket, (sockaddr*)&addr, &addrLen) == 0) {
        return std::string(inet_ntoa(addr.sin_addr));
    }
    
    return "";
}


uint16_t Socket::getPeerPort() const {
    if (!m_isValid) return 0;
    
    sockaddr_in addr;
    socklen_t addrLen = sizeof(addr);
    
    if (getpeername(m_socket, (sockaddr*)&addr, &addrLen) == 0) {
        return ntohs(addr.sin_port);
    }
    
    return 0;
}