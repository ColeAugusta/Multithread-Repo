#include "../include/platform_wrapper.h"

// Mutex implementation

Mutex::Mutex() : m_initialized(false) {
#ifdef _WIN32
    InitializeCriticalSection(&m_mutex);
    m_initialized = true;
#else
    int result = pthread_mutex_init(&m_mutex, nullptr);
    m_initialized = (result == 0);
#endif
}

Mutex::~Mutex() {
    if (m_initialized) {
#ifdef _WIN32
        DeleteCriticalSection(&m_mutex);
#else
        pthread_mutex_destroy(&m_mutex);
#endif
    }
}


void Mutex::lock() {
    if (!m_initialized) return;
    
#ifdef _WIN32
    EnterCriticalSection(&m_mutex);
#else
    pthread_mutex_lock(&m_mutex);
#endif
}


void Mutex::unlock() {
    if (!m_initialized) return;
    
#ifdef _WIN32
    LeaveCriticalSection(&m_mutex);
#else
    pthread_mutex_unlock(&m_mutex);
#endif
}


bool Mutex::tryLock() {
    if (!m_initialized) return false;
    
#ifdef _WIN32
    return TryEnterCriticalSection(&m_mutex) != 0;
#else
    return pthread_mutex_trylock(&m_mutex) == 0;
#endif
}