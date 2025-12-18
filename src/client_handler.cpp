#include "../include/client_handler.h"
#include "../include/file_manager.h"
#include <iostream>
#include <cstring>


// CLient Hndler for API implementation 



ClientHandler::ClientHandler(Socket* clientSocket, FileManager* fileManager, uint32_t clientId,
    const std::string& passwordHash)
    : m_clientSocket(clientSocket), m_fileManager(fileManager), m_clientId(clientId),
      m_running(false), m_uploadExpectedSize(0), m_uploadReceivedSize(0),
      m_serverPasswordHash(passwordHash), m_authenticated(false), m_failedAttempts(0) {
    m_lastActivity = time(nullptr);
}

// also deletes client socket with handler
ClientHandler::~ClientHandler() {
    if (m_uploadFile.is_open()) {
        m_uploadFile.close();
    }
    delete m_clientSocket;
}


void ClientHandler::run() {
    m_running = true;
    std::cout << "[Client " << m_clientId << "] Handler started" << std::endl;
    handleClient();
    m_running = false;
    std::cout << "[Client " << m_clientId << "] Handler finished" << std::endl;
}


// main function along with handleMessage
void ClientHandler::handleClient() {
    uint8_t headerBuffer[8];
    
    while (true) {
        // message header
        int bytesReceived = m_clientSocket->receive(headerBuffer, sizeof(headerBuffer));
        if (bytesReceived <= 0) {
            std::cout << "[Client " << m_clientId << "] Disconnected (no data)" << std::endl;
            break;
        }
        
        if (bytesReceived != sizeof(headerBuffer)) {
            std::cerr << "[Client " << m_clientId << "] Incomplete header received" << std::endl;
            break;
        }
        
        Protocol::MessageHeader header;
        if (!ProtocolHelper::deserializeHeader(headerBuffer, sizeof(headerBuffer), header)) {
            std::cerr << "[Client " << m_clientId << "] Invalid header received" << std::endl;
            sendErrorResponse("Invalid message header");
            break;
        }
        
        std::cout << "[Client " << m_clientId << "] Received message type: 0x" << std::hex 
                  << (int)header.messageType << std::dec << ", payload: " 
                  << header.payloadLength << " bytes" << std::endl;
        
        // get payload if present
        std::vector<uint8_t> payload;
        if (header.payloadLength > 0) {
            payload.resize(header.payloadLength);
            int totalReceived = 0;
            while (totalReceived < header.payloadLength) {
                int received = m_clientSocket->receive(payload.data() + totalReceived, 
                                                      header.payloadLength - totalReceived);
                if (received <= 0) {
                    std::cerr << "[Client " << m_clientId << "] Failed to receive payload" << std::endl;
                    return;
                }
                totalReceived += received;
            }
        }
        
        bool shouldContinue = handleMessage(header.messageType, payload);
        if (!shouldContinue) {
            break;
        }
    }
}

// handle client's message to server
bool ClientHandler::handleMessage(uint8_t messageType, const std::vector<uint8_t>& payload) {
    updateActivity();
        
    // checks time with handler
    if (checkTimeout()) {
        std::cout << "[Client " << m_clientId << "] Timeout - disconnecting" << std::endl;
        return false;
    }
    
    switch (messageType) {
        case Protocol::MSG_CONNECT_REQUEST:
            return handleAuthentication(payload);
            
        case Protocol::MSG_DISCONNECT:
            std::cout << "[Client " << m_clientId << "] Requested disconnect" << std::endl;
            return false;
            
        // ALL these operations require Authentication from user
        case Protocol::MSG_LIST_FILES:
        case Protocol::MSG_DOWNLOAD_REQUEST:
        case Protocol::MSG_UPLOAD_REQUEST:
        case Protocol::MSG_UPLOAD_DATA:
        case Protocol::MSG_UPLOAD_COMPLETE:
        case Protocol::MSG_DELETE_REQUEST:
            if (!checkAuthenticated()) {
                sendErrorResponse("Not authenticated - password required");
                return false;
            }
            // Fall through to original handlers
            break;
            
        default:
            std::cerr << "[Client " << m_clientId << "] Unknown message type: 0x" 
                    << std::hex << (int)messageType << std::dec << std::endl;
            sendErrorResponse("Unknown message type");
            return true;
    }
    
    // original message handling
    switch (messageType) {
        case Protocol::MSG_LIST_FILES:
            return handleListFiles();
        case Protocol::MSG_DOWNLOAD_REQUEST:
            return handleDownloadRequest(payload);
        case Protocol::MSG_UPLOAD_REQUEST:
            return handleUploadRequest(payload);
        case Protocol::MSG_UPLOAD_DATA:
            return handleUploadData(payload);
        case Protocol::MSG_UPLOAD_COMPLETE:
            return handleUploadComplete(payload);
        case Protocol::MSG_DELETE_REQUEST:
            return handleDeleteRequest(payload);
        default:
            return true;
    }
}


// handle Authentication request from client
bool ClientHandler::handleAuthentication(const std::vector<uint8_t>& payload) {
    std::string clientPasswordHash;
    size_t bytesRead;
    
    if (!ProtocolHelper::deserializeString(payload.data(), payload.size(), 
                                          clientPasswordHash, bytesRead)) {
        sendErrorResponse("Invalid password format");
        return true;
    }
    
    if (clientPasswordHash == m_serverPasswordHash) {
        m_authenticated = true;
        m_failedAttempts = 0;
        
        std::cout << "[Client " << m_clientId << "] Authentication successful" << std::endl;
        
        std::string welcomeMsg = "Authentication successful - Welcome to File Server";
        auto responsePayload = ProtocolHelper::createTextPayload(welcomeMsg);
        return sendMessage(Protocol::MSG_CONNECT_RESPONSE, responsePayload);
    } else {
        m_failedAttempts++;
        
        std::cout << "[Client " << m_clientId << "] Authentication FAILED (attempt " 
                  << m_failedAttempts << ")" << std::endl;
        
        if (m_failedAttempts >= 3) {
            sendErrorResponse("Too many failed attempts - disconnecting");
            return false; // Disconnect after 3 failures
        }
        
        sendErrorResponse("Invalid password");
        return true;
    }
}

bool ClientHandler::checkAuthenticated() {
    return m_authenticated;
}

void ClientHandler::updateActivity() {
    m_lastActivity = time(nullptr);
}

// uses time(nullptr) to get seconds, feeds into protocol timeout
bool ClientHandler::checkTimeout() {
    time_t now = time(nullptr);
    return (now - m_lastActivity) > Protocol::CONNECTION_TIMEOUT_SECONDS;
}


// creates a client to connect to server
bool ClientHandler::handleConnectRequest(const std::vector<uint8_t>& payload) {
    std::string clientName = "Anonymous";
    if (!payload.empty()) {
        size_t bytesRead;
        ProtocolHelper::deserializeString(payload.data(), payload.size(), clientName, bytesRead);
    }
    
    std::cout << "[Client " << m_clientId << "] Connect request from: " << clientName << std::endl;
    
    std::string welcomeMsg = "Welcome to Multi-Threaded File Server";
    auto responsePayload = ProtocolHelper::createTextPayload(welcomeMsg);
    return sendMessage(Protocol::MSG_CONNECT_RESPONSE, responsePayload);
}

bool ClientHandler::handleListFiles() {
    std::cout << "[Client " << m_clientId << "] List files request" << std::endl;
    
    std::vector<Protocol::FileInfo> files = m_fileManager->getFileList();
    
    // get payload size
    size_t payloadSize = sizeof(uint32_t);
    for (const auto& file : files) {
        payloadSize += sizeof(uint32_t) + file.filename.length();
        payloadSize += sizeof(uint64_t);
        payloadSize += sizeof(uint64_t);
    }
    
    // build payload
    std::vector<uint8_t> payload(payloadSize);
    size_t offset = 0;
    
    uint32_t fileCount = htonl(static_cast<uint32_t>(files.size()));
    std::memcpy(payload.data() + offset, &fileCount, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    
    // write each file
    for (const auto& file : files) {
        size_t written = ProtocolHelper::serializeFileInfo(file, payload.data() + offset, 
                                                           payloadSize - offset);
        if (written == 0) {
            sendErrorResponse("Failed to serialize file list");
            return true;
        }
        offset += written;
    }
    
    std::cout << "[Client " << m_clientId << "] Sending list of " << files.size() << " files" << std::endl;
    return sendMessage(Protocol::MSG_FILE_LIST_RESPONSE, payload);
}


bool ClientHandler::handleDownloadRequest(const std::vector<uint8_t>& payload) {
    std::string filename;
    size_t bytesRead;
    if (!ProtocolHelper::deserializeString(payload.data(), payload.size(), filename, bytesRead)) {
        sendErrorResponse("Invalid filename");
        return true;
    }

    // use security handler to validate filename
    if (!SecurityHelper::isValidFilename(filename)) {
        sendErrorResponse("Invalid filename");
        std::cout << "[Client " << m_clientId << "] SECURITY ALERT: Rejected filename: " 
                  << filename << std::endl;
        return true;
    }
    
    std::cout << "[Client " << m_clientId << "] Download request for: " << filename << std::endl;
    
    std::ifstream file;
    if (!m_fileManager->openForReading(filename, file)) {
        sendErrorResponse("File not found");
        return true;
    }
    
    file.seekg(0, std::ios::end);
    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    
    // send file data in chunks
    const size_t CHUNK_SIZE = 4096;
    std::vector<uint8_t> buffer(CHUNK_SIZE);
    size_t totalSent = 0;
    
    while (totalSent < fileSize) {
        size_t toRead = std::min(CHUNK_SIZE, static_cast<size_t>(fileSize - totalSent));
        file.read(reinterpret_cast<char*>(buffer.data()), toRead);
        
        if (!sendMessage(Protocol::MSG_DOWNLOAD_DATA, 
                       std::vector<uint8_t>(buffer.begin(), buffer.begin() + toRead))) {
            std::cerr << "[Client " << m_clientId << "] Failed to send file chunk" << std::endl;
            file.close();
            return false;
        }
        
        totalSent += toRead;
    }
    
    file.close();
    
    auto completePayload = ProtocolHelper::createStatusPayload(Protocol::STATUS_OK);
    sendMessage(Protocol::MSG_DOWNLOAD_COMPLETE, completePayload);
    
    std::cout << "[Client " << m_clientId << "] Download complete: " << filename 
              << " (" << fileSize << " bytes)" << std::endl;
    return true;
}

bool ClientHandler::handleUploadRequest(const std::vector<uint8_t>& payload) {
    std::string filename;
    size_t bytesRead;
    if (!ProtocolHelper::deserializeString(payload.data(), payload.size(), filename, bytesRead)) {
        sendErrorResponse("Invalid filename");
        return true;
    }
    
    // Use security to validate filename
    if (!SecurityHelper::isValidFilename(filename)) {
        sendErrorResponse("Invalid filename - may contain path traversal or illegal characters");
        std::cout << "[Client " << m_clientId << "] SECURITY ALERT: Rejected filename: " 
                  << filename << std::endl;
        return true;
    }

    uint64_t fileSize = ProtocolHelper::deserializeUint64(payload.data() + bytesRead);
    
    // Use security to validate filesize
    if (!SecurityHelper::isValidFileSize(fileSize)) {
        sendErrorResponse("File too large - maximum 1GB allowed");
        std::cout << "[Client " << m_clientId << "] SECURITY ALERT: Rejected large file: " 
                  << fileSize << " bytes" << std::endl;
        return true;
    }


    std::cout << "[Client " << m_clientId << "] Upload request for: " << filename 
              << " (" << fileSize << " bytes)" << std::endl;
    
    if (!m_fileManager->openForWriting(filename, m_uploadFile)) {
        sendErrorResponse("Cannot create file");
        return true;
    }
    
    // store upload state
    m_uploadFilename = filename;
    m_uploadExpectedSize = fileSize;
    m_uploadReceivedSize = 0;
    
    auto okPayload = ProtocolHelper::createStatusPayload(Protocol::STATUS_OK);
    sendMessage(Protocol::MSG_CONNECT_RESPONSE, okPayload);
    
    std::cout << "[Client " << m_clientId << "] Ready to receive upload data..." << std::endl;
    return true;
}

// writes chunks to file
bool ClientHandler::handleUploadData(const std::vector<uint8_t>& payload) {
    if (!m_uploadFile.is_open()) {
        sendErrorResponse("No active upload");
        return true;
    }
    
    m_uploadFile.write(reinterpret_cast<const char*>(payload.data()), payload.size());
    m_uploadReceivedSize += payload.size();
    
    return true;
}

bool ClientHandler::handleUploadComplete(const std::vector<uint8_t>& payload) {
    if (m_uploadFile.is_open()) {
        m_uploadFile.close();
        std::cout << "[Client " << m_clientId << "] Upload complete: " << m_uploadFilename 
                  << " (" << m_uploadReceivedSize << " bytes received)" << std::endl;
    }
    
    // clear upload state
    m_uploadFilename.clear();
    m_uploadExpectedSize = 0;
    m_uploadReceivedSize = 0;
    
    return true;
}

bool ClientHandler::handleDeleteRequest(const std::vector<uint8_t>& payload) {
    std::string filename;
    size_t bytesRead;
    if (!ProtocolHelper::deserializeString(payload.data(), payload.size(), filename, bytesRead)) {
        sendErrorResponse("Invalid filename");
        return true;
    }

    if (!SecurityHelper::isValidFilename(filename)) {
        sendErrorResponse("Invalid filename");
        std::cout << "[Client " << m_clientId << "] SECURITY ALERT: Rejected filename: " 
                  << filename << std::endl;
        return true;
    }
    
    std::cout << "[Client " << m_clientId << "] Delete request for: " << filename << std::endl;
    
    if (m_fileManager->deleteFile(filename)) {
        auto okPayload = ProtocolHelper::createStatusPayload(Protocol::STATUS_OK, "File deleted");
        sendMessage(Protocol::MSG_DELETE_RESPONSE, okPayload);
        std::cout << "[Client " << m_clientId << "] File deleted: " << filename << std::endl;
    } else {
        sendErrorResponse("Failed to delete file");
    }
    
    return true;
}

bool ClientHandler::sendMessage(uint8_t messageType, const std::vector<uint8_t>& payload) {
    Protocol::MessageHeader header(messageType, static_cast<uint32_t>(payload.size()));
    
    uint8_t headerBuffer[8];
    if (!ProtocolHelper::serializeHeader(header, headerBuffer, sizeof(headerBuffer))) {
        return false;
    }
    
    if (m_clientSocket->send(headerBuffer, sizeof(headerBuffer)) != sizeof(headerBuffer)) {
        return false;
    }
    
    if (!payload.empty()) {
        if (m_clientSocket->send(payload.data(), payload.size()) != static_cast<int>(payload.size())) {
            return false;
        }
    }
    
    return true;
}

void ClientHandler::sendErrorResponse(const std::string& errorMsg) {
    auto payload = ProtocolHelper::createStatusPayload(Protocol::STATUS_ERROR, errorMsg);
    sendMessage(Protocol::MSG_ERROR_RESPONSE, payload);
}