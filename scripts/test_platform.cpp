#include "platform_wrapper.h"
#include <iostream>
#include <cstring>

// Test thread function
ThreadReturn THREAD_CALL testThreadFunction(void* arg) {
    int* value = static_cast<int*>(arg);
    std::cout << "Thread " << Thread::getCurrentThreadId() 
              << " started with value: " << *value << std::endl;
    
    Thread::sleep(1000);
    
    std::cout << "Thread " << Thread::getCurrentThreadId() 
              << " finished" << std::endl;
    
#ifdef _WIN32
    return 0;
#else
    return nullptr;
#endif
}

// Simple echo server for testing
void runEchoServer(uint16_t port) {
    Socket serverSocket;
    
    if (!serverSocket.create()) {
        std::cerr << "Failed to create socket: " << serverSocket.getLastError() << std::endl;
        return;
    }
    
    serverSocket.setReuseAddr(true);
    
    if (!serverSocket.bind(port)) {
        std::cerr << "Failed to bind to port " << port << ": " 
                  << serverSocket.getLastError() << std::endl;
        return;
    }
    
    if (!serverSocket.listen()) {
        std::cerr << "Failed to listen: " << serverSocket.getLastError() << std::endl;
        return;
    }
    
    std::cout << "Echo server listening on port " << port << std::endl;
    std::cout << "Waiting for connection..." << std::endl;
    
    Socket* clientSocket = serverSocket.accept();
    if (!clientSocket) {
        std::cerr << "Failed to accept connection" << std::endl;
        return;
    }
    
    std::cout << "Client connected from " << clientSocket->getPeerAddress() 
              << ":" << clientSocket->getPeerPort() << std::endl;
    
    char buffer[1024];
    while (true) {
        int bytesReceived = clientSocket->receive(buffer, sizeof(buffer) - 1);
        if (bytesReceived <= 0) {
            std::cout << "Client disconnected" << std::endl;
            break;
        }
        
        buffer[bytesReceived] = '\0';
        std::cout << "Received: " << buffer;
        
        // Echo back
        int bytesSent = clientSocket->send(buffer, bytesReceived);
        if (bytesSent <= 0) {
            std::cerr << "Failed to send data" << std::endl;
            break;
        }
    }
    
    delete clientSocket;
}

// Simple echo client for testing
void runEchoClient(const std::string& host, uint16_t port) {
    Socket clientSocket;
    
    if (!clientSocket.create()) {
        std::cerr << "Failed to create socket: " << clientSocket.getLastError() << std::endl;
        return;
    }
    
    std::cout << "Connecting to " << host << ":" << port << "..." << std::endl;
    
    if (!clientSocket.connect(host, port)) {
        std::cerr << "Failed to connect: " << clientSocket.getLastError() << std::endl;
        return;
    }
    
    std::cout << "Connected to server!" << std::endl;
    std::cout << "Type messages to send (empty line to quit):" << std::endl;
    
    std::string line;
    char buffer[1024];
    
    while (true) {
        std::cout << "> ";
        std::getline(std::cin, line);
        
        if (line.empty()) break;
        
        line += "\n";
        int bytesSent = clientSocket.send(line.c_str(), line.length());
        if (bytesSent <= 0) {
            std::cerr << "Failed to send data" << std::endl;
            break;
        }
        
        int bytesReceived = clientSocket.receive(buffer, sizeof(buffer) - 1);
        if (bytesReceived <= 0) {
            std::cerr << "Connection closed by server" << std::endl;
            break;
        }
        
        buffer[bytesReceived] = '\0';
        std::cout << "Echo: " << buffer;
    }
    
    std::cout << "Disconnecting..." << std::endl;
}

void testMutex() {
    std::cout << "\n=== Testing Mutex ===" << std::endl;
    
    Mutex mutex;
    
    std::cout << "Locking mutex..." << std::endl;
    mutex.lock();
    std::cout << "Mutex locked" << std::endl;
    
    std::cout << "Trying to lock again (should fail)..." << std::endl;
    if (mutex.tryLock()) {
        std::cout << "TryLock succeeded (unexpected!)" << std::endl;
        mutex.unlock();
    } else {
        std::cout << "TryLock failed as expected" << std::endl;
    }
    
    std::cout << "Unlocking mutex..." << std::endl;
    mutex.unlock();
    std::cout << "Mutex unlocked" << std::endl;
    
    std::cout << "Testing LockGuard..." << std::endl;
    {
        LockGuard guard(mutex);
        std::cout << "Mutex locked via LockGuard" << std::endl;
    }
    std::cout << "Mutex unlocked via LockGuard destructor" << std::endl;
}

void testThreads() {
    std::cout << "\n=== Testing Threads ===" << std::endl;
    
    int value1 = 42;
    int value2 = 100;
    
    Thread thread1, thread2;
    
    std::cout << "Starting thread 1..." << std::endl;
    thread1.start(testThreadFunction, &value1);
    
    std::cout << "Starting thread 2..." << std::endl;
    thread2.start(testThreadFunction, &value2);
    
    std::cout << "Main thread sleeping..." << std::endl;
    Thread::sleep(500);
    
    std::cout << "Waiting for threads to complete..." << std::endl;
    thread1.join();
    thread2.join();
    
    std::cout << "All threads completed" << std::endl;
}

void testByteOrder() {
    std::cout << "\n=== Testing Byte Order Conversion ===" << std::endl;
    
    uint16_t port = 8080;
    uint32_t addr = 0x12345678;
    
    uint16_t netPort = PlatformUtils::hostToNetwork16(port);
    uint32_t netAddr = PlatformUtils::hostToNetwork32(addr);
    
    std::cout << "Host port: " << port << " -> Network: " << netPort << std::endl;
    std::cout << "Host addr: 0x" << std::hex << addr << " -> Network: 0x" << netAddr << std::dec << std::endl;
    
    uint16_t hostPort = PlatformUtils::networkToHost16(netPort);
    uint32_t hostAddr = PlatformUtils::networkToHost32(netAddr);
    
    std::cout << "Converted back - Port: " << hostPort << ", Addr: 0x" << std::hex << hostAddr << std::dec << std::endl;
    
    if (port == hostPort && addr == hostAddr) {
        std::cout << "Byte order conversion: PASSED" << std::endl;
    } else {
        std::cout << "Byte order conversion: FAILED" << std::endl;
    }
}

void printUsage(const char* progName) {
    std::cout << "Usage:" << std::endl;
    std::cout << "  " << progName << " test          - Run all unit tests" << std::endl;
    std::cout << "  " << progName << " server [port] - Run echo server (default port 8080)" << std::endl;
    std::cout << "  " << progName << " client <host> [port] - Run echo client" << std::endl;
}

int main(int argc, char* argv[]) {
    // Initialize platform
    if (!PlatformUtils::initialize()) {
        std::cerr << "Failed to initialize platform layer" << std::endl;
        return 1;
    }
    
    if (argc < 2) {
        printUsage(argv[0]);
        PlatformUtils::cleanup();
        return 1;
    }
    
    std::string command = argv[1];
    
    if (command == "test") {
        std::cout << "=== Platform Abstraction Layer Tests ===" << std::endl;
        testByteOrder();
        testMutex();
        testThreads();
        std::cout << "\n=== All tests completed ===" << std::endl;
        
    } else if (command == "server") {
        uint16_t port = 8080;
        if (argc >= 3) {
            port = static_cast<uint16_t>(std::atoi(argv[2]));
        }
        runEchoServer(port);
        
    } else if (command == "client") {
        if (argc < 3) {
            std::cerr << "Error: client requires host argument" << std::endl;
            printUsage(argv[0]);
            PlatformUtils::cleanup();
            return 1;
        }
        
        std::string host = argv[2];
        uint16_t port = 8080;
        if (argc >= 4) {
            port = static_cast<uint16_t>(std::atoi(argv[3]));
        }
        runEchoClient(host, port);
        
    } else {
        std::cerr << "Unknown command: " << command << std::endl;
        printUsage(argv[0]);
        PlatformUtils::cleanup();
        return 1;
    }
    
    PlatformUtils::cleanup();
    return 0;
}