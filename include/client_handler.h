#ifndef CLIENT_HANDLER_H
#define CLIENT_HANDLER_H

#include "platform_wrapper.h"
#include "protocol.h"
#include <string>
#include <fstream>
#include <vector>

class FileManager;

class ClientHandler {
public:
    ClientHandler(Socket* clientSocket, FileManager* fileManager, uint32_t clientId, const std::string& passwordHash);
    ~ClientHandler();
    
    void run();
    bool isRunning() const { return m_running; }
    uint32_t getClientId() const { return m_clientId; }
    
private:
    void handleClient();
    bool handleMessage(uint8_t messageType, const std::vector<uint8_t>& payload);
    
    bool handleConnectRequest(const std::vector<uint8_t>& payload);
    bool handleListFiles();
    bool handleDownloadRequest(const std::vector<uint8_t>& payload);
    bool handleUploadRequest(const std::vector<uint8_t>& payload);
    bool handleUploadData(const std::vector<uint8_t>& payload);
    bool handleUploadComplete(const std::vector<uint8_t>& payload);
    bool handleDeleteRequest(const std::vector<uint8_t>& payload);
    
    bool sendMessage(uint8_t messageType, const std::vector<uint8_t>& payload);
    void sendErrorResponse(const std::string& errorMsg);

    std::string m_serverPasswordHash;
    bool m_authenticated;
    time_t m_lastActivity;
    int m_failedAttempts;

    bool handleAuthentication(const std::vector<uint8_t>& payload);
    bool checkAuthenticated();
    void updateActivity();
    bool checkTimeout();
    
    Socket* m_clientSocket;
    FileManager* m_fileManager;
    uint32_t m_clientId;
    bool m_running;
    
    std::ofstream m_uploadFile;
    std::string m_uploadFilename;
    uint64_t m_uploadExpectedSize;
    uint64_t m_uploadReceivedSize;
};

#endif