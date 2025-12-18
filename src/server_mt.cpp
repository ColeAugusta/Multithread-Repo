#include "../include/platform_wrapper.h"
#include "../include/protocol.h"
#include "../include/file_manager.h"
#include "../include/client_handler.h"
#include <iostream>
#include <vector>
#include <map>

// Multi-threaded server

// thread entry point wrapper
ThreadReturn THREAD_CALL clientThreadFunction(void* arg) {
    ClientHandler* handler = static_cast<ClientHandler*>(arg);
    handler->run();
    
#ifdef _WIN32
    return 0;
#else
    return nullptr;
#endif
}

class MultiThreadedServer {
public:
    MultiThreadedServer(uint16_t port, const std::string& storageDir, int maxClients = 10, const std::string& password = "admin123")
        : m_port(port), m_fileManager(storageDir), m_running(false), 
          m_maxClients(maxClients), m_nextClientId(1),
          m_passwordHash(SecurityHelper::hashPassword(password)) {
    }
    
    ~MultiThreadedServer() {
        stop();
    }
    
    bool start() {
        if (!m_serverSocket.create()) {
            std::cerr << "Failed to create server socket: " << m_serverSocket.getLastError() << std::endl;
            return false;
        }
        
        m_serverSocket.setReuseAddr(true);
        
        if (!m_serverSocket.bind(m_port)) {
            std::cerr << "Failed to bind to port " << m_port << ": " << m_serverSocket.getLastError() << std::endl;
            return false;
        }
        
        if (!m_serverSocket.listen(m_maxClients)) {
            std::cerr << "Failed to listen: " << m_serverSocket.getLastError() << std::endl;
            return false;
        }
        
        std::cout << "========================================" << std::endl;
        std::cout << "Multi-Threaded File Server Started" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Port: " << m_port << std::endl;
        std::cout << "Storage Directory: " << m_fileManager.getStorageDir() << std::endl;
        std::cout << "Max Concurrent Clients: " << m_maxClients << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Waiting for connections..." << std::endl;
        
        m_running = true;
        return true;
    }
    
    void run() {
        while (m_running) {
            Socket* clientSocket = m_serverSocket.accept();
            if (!clientSocket) {
                std::cerr << "Failed to accept connection" << std::endl;
                continue;
            }
            
            cleanupFinishedClients();
            
            {
                LockGuard lock(m_clientsMutex);
                if (m_clients.size() >= static_cast<size_t>(m_maxClients)) {
                    std::cerr << "Maximum clients reached, rejecting connection from " 
                            << clientSocket->getPeerAddress() << std::endl;
                    delete clientSocket;
                    continue;
                }
            }
            
            uint32_t clientId = m_nextClientId++;
            
            std::cout << "\n[Server] New connection from " << clientSocket->getPeerAddress() 
                    << ":" << clientSocket->getPeerPort() 
                    << " (Client ID: " << clientId << ")" << std::endl;
            
            ClientHandler* handler = new ClientHandler(clientSocket, &m_fileManager, 
                                                    clientId, m_passwordHash);
            
            Thread* clientThread = new Thread();
            if (clientThread->start(clientThreadFunction, handler)) {
                LockGuard lock(m_clientsMutex);
                m_clients[clientId] = ClientInfo{handler, clientThread};
                std::cout << "[Server] Active clients: " << m_clients.size() << std::endl;
            } else {
                std::cerr << "[Server] Failed to create thread for client " << clientId << std::endl;
                delete handler;
                delete clientThread;
            }
        }
        
        std::cout << "\n[Server] Shutting down..." << std::endl;
        waitForAllClients();
    }
    
    void stop() {
        if (m_running) {
            m_running = false;
            m_serverSocket.close();
            waitForAllClients();
        }
    }
    
private:
    std::string m_passwordHash;

    struct ClientInfo {
        ClientHandler* handler;
        Thread* thread;
    };
    
    void cleanupFinishedClients() {
        LockGuard lock(m_clientsMutex);
        
        auto it = m_clients.begin();
        while (it != m_clients.end()) {
            if (!it->second.handler->isRunning()) {
                std::cout << "[Server] Cleaning up client " << it->first << std::endl;
                
                it->second.thread->join();
                
                delete it->second.handler;
                delete it->second.thread;
                
                it = m_clients.erase(it);
            } else {
                ++it;
            }
        }
    }
    
    void waitForAllClients() {
        std::cout << "[Server] Waiting for all clients to disconnect..." << std::endl;
        
        LockGuard lock(m_clientsMutex);
        for (auto& pair : m_clients) {
            std::cout << "[Server] Waiting for client " << pair.first << std::endl;
            pair.second.thread->join();
            delete pair.second.handler;
            delete pair.second.thread;
        }
        m_clients.clear();
        
        std::cout << "[Server] All clients disconnected" << std::endl;
    }
    
    Socket m_serverSocket;
    uint16_t m_port;
    FileManager m_fileManager;
    bool m_running;
    int m_maxClients;
    uint32_t m_nextClientId;
    
    std::map<uint32_t, ClientInfo> m_clients;
    Mutex m_clientsMutex;
};

void printUsage(const char* progName) {
    std::cout << "Usage: " << progName << " [port] [storage_dir] [max_clients]" << std::endl;
    std::cout << "  port        - Server port (default: 8080)" << std::endl;
    std::cout << "  storage_dir - Storage directory (default: server_files)" << std::endl;
    std::cout << "  max_clients - Maximum concurrent clients (default: 10)" << std::endl;
}

int main(int argc, char* argv[]) {
    if (!PlatformUtils::initialize()) {
        std::cerr << "Failed to initialize platform" << std::endl;
        return 1;
    }
    
    uint16_t port = 8080;
    std::string storageDir = "server_files";
    int maxClients = 10;
    std::string password = "admin123";
    
    if (argc >= 2) {
        port = static_cast<uint16_t>(std::atoi(argv[1]));
    }
    if (argc >= 3) {
        storageDir = argv[2];
    }
    if (argc >= 4) {
        maxClients = std::atoi(argv[3]);
    }
    if (argc >= 5) {
        password = argv[4];
    }
    
    std::cout << "Server password hash: " << SecurityHelper::hashPassword(password) << std::endl;
    std::cout << "IMPORTANT: Change default password for production use!" << std::endl;
    MultiThreadedServer server(port, storageDir, maxClients, password);
    
    if (!server.start()) {
        PlatformUtils::cleanup();
        return 1;
    }
    
    server.run();
    
    PlatformUtils::cleanup();
    return 0;
}