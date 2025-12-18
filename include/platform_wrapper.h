#ifndef PLATFORM_WRAPPER_H
#define PLATFORM_WRAPPER_H

#include <string>
#include <cstdint>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #pragma comment(lib, "ws2_32.lib")
    
    typedef SOCKET SocketHandle;
    typedef HANDLE ThreadHandle;
    typedef CRITICAL_SECTION MutexHandle;
    typedef DWORD ThreadReturn;
    
    #define INVALID_SOCKET_HANDLE INVALID_SOCKET
    #define SOCKET_ERROR_CODE WSAGetLastError()
    #define THREAD_CALL WINAPI
    
#else
    #include <sys/socket.h>
    #include <sys/types.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <netdb.h>
    #include <pthread.h>
    #include <errno.h>
    #include <fcntl.h>
    
    typedef int SocketHandle;
    typedef pthread_t ThreadHandle;
    typedef pthread_mutex_t MutexHandle;
    typedef void* ThreadReturn;
    
    #define INVALID_SOCKET_HANDLE -1
    #define SOCKET_ERROR_CODE errno
    #define THREAD_CALL
#endif

class Socket;
class Thread;
class Mutex;

// use correct thread library
#ifdef _WIN32
    typedef DWORD (WINAPI *ThreadFunction)(void*);
#else
    typedef void* (*ThreadFunction)(void*);
#endif


class Socket {
public:
    Socket();
    explicit Socket(SocketHandle handle);
    ~Socket();
    
    // no copying
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    
    // allow moving
    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;
    
    bool create();
    bool bind(uint16_t port, const std::string& address = "0.0.0.0");
    bool listen(int backlog = 5);
    Socket* accept();
    bool connect(const std::string& host, uint16_t port);
    
    int send(const void* data, size_t length);
    int receive(void* buffer, size_t length);
    
    bool setNonBlocking(bool nonBlocking);
    bool setReuseAddr(bool reuse);
    
    void close();
    bool isValid() const;
    
    SocketHandle getHandle() const { return m_socket; }
    std::string getLastError() const;
    
    std::string getPeerAddress() const;
    uint16_t getPeerPort() const;
    
    static bool initializeSockets();
    static void cleanupSockets();
    
private:
    SocketHandle m_socket;
    bool m_isValid;
    
    static bool s_initialized;
};


class Thread {
public:
    Thread();
    ~Thread();
    
    // no copying
    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;
    
    bool start(ThreadFunction func, void* arg);
    bool join();
    bool detach();
    
    bool isRunning() const { return m_running; }
    ThreadHandle getHandle() const { return m_thread; }
    
    static uint64_t getCurrentThreadId();
    static void sleep(uint32_t milliseconds);
    
private:
    ThreadHandle m_thread;
    bool m_running;
    bool m_detached;
};


class Mutex {
public:
    Mutex();
    ~Mutex();
    
    // no copying
    Mutex(const Mutex&) = delete;
    Mutex& operator=(const Mutex&) = delete;
    
    void lock();
    void unlock();
    bool tryLock();
    
    MutexHandle& getHandle() { return m_mutex; }
    
private:
    MutexHandle m_mutex;
    bool m_initialized;
};

// lockguard class for mutex
class LockGuard {
public:
    explicit LockGuard(Mutex& mutex) : m_mutex(mutex) {
        m_mutex.lock();
    }
    
    ~LockGuard() {
        m_mutex.unlock();
    }
    
    LockGuard(const LockGuard&) = delete;
    LockGuard& operator=(const LockGuard&) = delete;
    
private:
    Mutex& m_mutex;
};


namespace PlatformUtils {
    bool initialize();
    void cleanup();
    
    std::string getLastErrorString();
    
    // network byte order conversion
    inline uint16_t hostToNetwork16(uint16_t value) { return htons(value); }
    inline uint32_t hostToNetwork32(uint32_t value) { return htonl(value); }
    inline uint16_t networkToHost16(uint16_t value) { return ntohs(value); }
    inline uint32_t networkToHost32(uint32_t value) { return ntohl(value); }
}

#endif