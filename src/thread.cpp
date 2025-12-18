#include "../include/platform_wrapper.h"

// Thread implementation


Thread::Thread() : m_running(false), m_detached(false) {
#ifdef _WIN32
    m_thread = nullptr;
#else
    m_thread = 0;
#endif
}

Thread::~Thread() {
    if (m_running && !m_detached) {
        join();
    }
}

bool Thread::start(ThreadFunction func, void* arg) {
    if (m_running) return false;
    
#ifdef _WIN32
    m_thread = CreateThread(nullptr, 0, func, arg, 0, nullptr);
    m_running = (m_thread != nullptr);
#else
    int result = pthread_create(&m_thread, nullptr, func, arg);
    m_running = (result == 0);
#endif
    
    return m_running;
}

bool Thread::join() {
    if (!m_running || m_detached) return false;
    
#ifdef _WIN32
    DWORD result = WaitForSingleObject(m_thread, INFINITE);
    if (result == WAIT_OBJECT_0) {
        CloseHandle(m_thread);
        m_running = false;
        return true;
    }
    return false;
#else
    int result = pthread_join(m_thread, nullptr);
    if (result == 0) {
        m_running = false;
        return true;
    }
    return false;
#endif
}

bool Thread::detach() {
    if (!m_running || m_detached) return false;
    
#ifdef _WIN32
    CloseHandle(m_thread);
    m_detached = true;
    return true;
#else
    int result = pthread_detach(m_thread);
    m_detached = (result == 0);
    return m_detached;
#endif
}

uint64_t Thread::getCurrentThreadId() {
#ifdef _WIN32
    return static_cast<uint64_t>(GetCurrentThreadId());
#else
    return static_cast<uint64_t>(pthread_self());
#endif
}

void Thread::sleep(uint32_t milliseconds) {
#ifdef _WIN32
    Sleep(milliseconds);
#else
    usleep(milliseconds * 1000);
#endif
}