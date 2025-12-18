#include "../include/platform_wrapper.h"
#include "../include/protocol.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <ctime>
#include <sys/stat.h>

// Single threaded server used for testing

#ifdef _WIN32
    #include <direct.h>
    #define mkdir _mkdir
    #define stat _stat
#else
    #include <dirent.h>
    #include <sys/types.h>
#endif

class FileServer {
public:
    FileServer(uint16_t port, const std::string& storageDir)
        : m_port(port), m_storageDir(storageDir), m_running(false) {
        createStorageDirectory();
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
        
        if (!m_serverSocket.listen(5)) {
            std::cerr << "Failed to listen: " << m_serverSocket.getLastError() << std::endl;
            return false;
        }
        
        std::cout << "File server started on port " << m_port << std::endl;
        std::cout << "Storage directory: " << m_storageDir << std::endl;
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
            
            std::cout << "\nClient connected from " << clientSocket->getPeerAddress() 
                      << ":" << clientSocket->getPeerPort() << std::endl;
            
            handleClient(clientSocket);
            
            delete clientSocket;
            std::cout << "Client disconnected" << std::endl;
        }
    }
    
    void stop() {
        m_running = false;
        m_serverSocket.close();
    }
    
private:
    void createStorageDirectory() {
#ifdef _WIN32
        mkdir(m_storageDir.c_str());
#else
        mkdir(m_storageDir.c_str(), 0755);
#endif
    }
    
    void handleClient(Socket* clientSocket) {
        uint8_t headerBuffer[8];
        
        while (true) {
            // Receive message header
            int bytesReceived = clientSocket->receive(headerBuffer, sizeof(headerBuffer));
            if (bytesReceived <= 0) {
                std::cout << "Client disconnected (no data)" << std::endl;
                break;
            }
            
            if (bytesReceived != sizeof(headerBuffer)) {
                std::cerr << "Incomplete header received" << std::endl;
                break;
            }
            
            // Parse header
            Protocol::MessageHeader header;
            if (!ProtocolHelper::deserializeHeader(headerBuffer, sizeof(headerBuffer), header)) {
                std::cerr << "Invalid header received" << std::endl;
                sendErrorResponse(clientSocket, "Invalid message header");
                break;
            }
            
            std::cout << "Received message type: 0x" << std::hex << (int)header.messageType 
                      << std::dec << ", payload length: " << header.payloadLength << std::endl;
            
            // Receive payload if present
            std::vector<uint8_t> payload;
            if (header.payloadLength > 0) {
                payload.resize(header.payloadLength);
                int totalReceived = 0;
                while (totalReceived < header.payloadLength) {
                    int received = clientSocket->receive(payload.data() + totalReceived, 
                                                        header.payloadLength - totalReceived);
                    if (received <= 0) {
                        std::cerr << "Failed to receive payload" << std::endl;
                        return;
                    }
                    totalReceived += received;
                }
            }
            
            // Handle message based on type
            bool shouldContinue = handleMessage(clientSocket, header.messageType, payload);
            if (!shouldContinue) {
                break;
            }
        }
    }
    
    bool handleMessage(Socket* clientSocket, uint8_t messageType, const std::vector<uint8_t>& payload) {
        switch (messageType) {
            case Protocol::MSG_CONNECT_REQUEST:
                return handleConnectRequest(clientSocket, payload);
                
            case Protocol::MSG_LIST_FILES:
                return handleListFiles(clientSocket);
                
            case Protocol::MSG_DOWNLOAD_REQUEST:
                return handleDownloadRequest(clientSocket, payload);
                
            case Protocol::MSG_UPLOAD_REQUEST:
                return handleUploadRequest(clientSocket, payload);
                
            case Protocol::MSG_UPLOAD_DATA:
                return handleUploadData(clientSocket, payload);
                
            case Protocol::MSG_UPLOAD_COMPLETE:
                return handleUploadComplete(clientSocket, payload);
                
            case Protocol::MSG_DELETE_REQUEST:
                return handleDeleteRequest(clientSocket, payload);
                
            case Protocol::MSG_DISCONNECT:
                std::cout << "Client requested disconnect" << std::endl;
                return false;
                
            default:
                std::cerr << "Unknown message type: 0x" << std::hex << (int)messageType << std::dec << std::endl;
                sendErrorResponse(clientSocket, "Unknown message type");
                return true;
        }
    }
    
    bool handleConnectRequest(Socket* clientSocket, const std::vector<uint8_t>& payload) {
        std::string clientName = "Anonymous";
        if (!payload.empty()) {
            size_t bytesRead;
            ProtocolHelper::deserializeString(payload.data(), payload.size(), clientName, bytesRead);
        }
        
        std::cout << "Connect request from: " << clientName << std::endl;
        
        // Send connect response
        std::string welcomeMsg = "Welcome to File Server";
        auto responsePayload = ProtocolHelper::createTextPayload(welcomeMsg);
        return sendMessage(clientSocket, Protocol::MSG_CONNECT_RESPONSE, responsePayload);
    }
    
    bool handleListFiles(Socket* clientSocket) {
        std::cout << "List files request" << std::endl;
        
        std::vector<Protocol::FileInfo> files = getFileList();
        
        // Calculate payload size
        size_t payloadSize = sizeof(uint32_t); // File count
        for (const auto& file : files) {
            payloadSize += sizeof(uint32_t) + file.filename.length(); // Filename
            payloadSize += sizeof(uint64_t); // File size
            payloadSize += sizeof(uint64_t); // Timestamp
        }
        
        // Build payload
        std::vector<uint8_t> payload(payloadSize);
        size_t offset = 0;
        
        // Write file count
        uint32_t fileCount = htonl(static_cast<uint32_t>(files.size()));
        std::memcpy(payload.data() + offset, &fileCount, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        
        // Write each file info
        for (const auto& file : files) {
            size_t written = ProtocolHelper::serializeFileInfo(file, payload.data() + offset, payloadSize - offset);
            if (written == 0) {
                sendErrorResponse(clientSocket, "Failed to serialize file list");
                return true;
            }
            offset += written;
        }
        
        std::cout << "Sending list of " << files.size() << " files" << std::endl;
        return sendMessage(clientSocket, Protocol::MSG_FILE_LIST_RESPONSE, payload);
    }
    
    bool handleDownloadRequest(Socket* clientSocket, const std::vector<uint8_t>& payload) {
        // Parse filename
        std::string filename;
        size_t bytesRead;
        if (!ProtocolHelper::deserializeString(payload.data(), payload.size(), filename, bytesRead)) {
            sendErrorResponse(clientSocket, "Invalid filename");
            return true;
        }
        
        std::cout << "Download request for: " << filename << std::endl;
        
        std::string filepath = m_storageDir + "/" + filename;
        
        // Check if file exists
        std::ifstream file(filepath, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            sendErrorResponse(clientSocket, "File not found");
            return true;
        }
        
        // Get file size
        std::streamsize fileSize = file.tellg();
        file.seekg(0, std::ios::beg);
        
        // Send file data in chunks
        const size_t CHUNK_SIZE = 4096;
        std::vector<uint8_t> buffer(CHUNK_SIZE);
        size_t totalSent = 0;
        
        while (totalSent < fileSize) {
            size_t toRead = std::min(CHUNK_SIZE, static_cast<size_t>(fileSize - totalSent));
            file.read(reinterpret_cast<char*>(buffer.data()), toRead);
            
            if (!sendMessage(clientSocket, Protocol::MSG_DOWNLOAD_DATA, 
                           std::vector<uint8_t>(buffer.begin(), buffer.begin() + toRead))) {
                std::cerr << "Failed to send file chunk" << std::endl;
                return false;
            }
            
            totalSent += toRead;
        }
        
        file.close();
        
        // Send download complete
        auto completePayload = ProtocolHelper::createStatusPayload(Protocol::STATUS_OK);
        sendMessage(clientSocket, Protocol::MSG_DOWNLOAD_COMPLETE, completePayload);
        
        std::cout << "Download complete: " << filename << " (" << fileSize << " bytes)" << std::endl;
        return true;
    }
    
    bool handleUploadRequest(Socket* clientSocket, const std::vector<uint8_t>& payload) {
        // Parse filename and file size
        std::string filename;
        size_t bytesRead;
        if (!ProtocolHelper::deserializeString(payload.data(), payload.size(), filename, bytesRead)) {
            sendErrorResponse(clientSocket, "Invalid filename");
            return true;
        }
        
        uint64_t fileSize = ProtocolHelper::deserializeUint64(payload.data() + bytesRead);
        
        std::cout << "Upload request for: " << filename << " (" << fileSize << " bytes)" << std::endl;
        
        std::string filepath = m_storageDir + "/" + filename;
        
        // Open file for writing
        m_uploadFile.open(filepath, std::ios::binary);
        if (!m_uploadFile.is_open()) {
            sendErrorResponse(clientSocket, "Cannot create file");
            return true;
        }
        
        // Store upload state
        m_uploadFilename = filename;
        m_uploadExpectedSize = fileSize;
        m_uploadReceivedSize = 0;
        
        // Send OK to proceed
        auto okPayload = ProtocolHelper::createStatusPayload(Protocol::STATUS_OK);
        sendMessage(clientSocket, Protocol::MSG_CONNECT_RESPONSE, okPayload);
        
        std::cout << "Ready to receive upload data..." << std::endl;
        return true;
    }
    
    bool handleUploadData(Socket* clientSocket, const std::vector<uint8_t>& payload) {
        if (!m_uploadFile.is_open()) {
            sendErrorResponse(clientSocket, "No active upload");
            return true;
        }
        
        // Write chunk to file
        m_uploadFile.write(reinterpret_cast<const char*>(payload.data()), payload.size());
        m_uploadReceivedSize += payload.size();
        
        return true;
    }
    
    bool handleUploadComplete(Socket* clientSocket, const std::vector<uint8_t>& payload) {
        if (m_uploadFile.is_open()) {
            m_uploadFile.close();
            std::cout << "Upload complete: " << m_uploadFilename 
                      << " (" << m_uploadReceivedSize << " bytes received)" << std::endl;
        }
        
        // Clear upload state
        m_uploadFilename.clear();
        m_uploadExpectedSize = 0;
        m_uploadReceivedSize = 0;
        
        return true;
    }
    
    bool handleDeleteRequest(Socket* clientSocket, const std::vector<uint8_t>& payload) {
        // Parse filename
        std::string filename;
        size_t bytesRead;
        if (!ProtocolHelper::deserializeString(payload.data(), payload.size(), filename, bytesRead)) {
            sendErrorResponse(clientSocket, "Invalid filename");
            return true;
        }
        
        std::cout << "Delete request for: " << filename << std::endl;
        
        std::string filepath = m_storageDir + "/" + filename;
        
        if (std::remove(filepath.c_str()) == 0) {
            auto okPayload = ProtocolHelper::createStatusPayload(Protocol::STATUS_OK, "File deleted");
            sendMessage(clientSocket, Protocol::MSG_DELETE_RESPONSE, okPayload);
            std::cout << "File deleted: " << filename << std::endl;
        } else {
            sendErrorResponse(clientSocket, "Failed to delete file");
        }
        
        return true;
    }
    
    std::vector<Protocol::FileInfo> getFileList() {
        std::vector<Protocol::FileInfo> files;
        
#ifdef _WIN32
        WIN32_FIND_DATAA findData;
        std::string searchPath = m_storageDir + "/*";
        HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
        
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    Protocol::FileInfo info;
                    info.filename = findData.cFileName;
                    info.fileSize = (static_cast<uint64_t>(findData.nFileSizeHigh) << 32) | findData.nFileSizeLow;
                    info.timestamp = static_cast<uint64_t>(time(nullptr));
                    files.push_back(info);
                }
            } while (FindNextFileA(hFind, &findData));
            FindClose(hFind);
        }
#else
        DIR* dir = opendir(m_storageDir.c_str());
        if (dir) {
            struct dirent* entry;
            while ((entry = readdir(dir)) != nullptr) {
                if (entry->d_type == DT_REG) {
                    std::string filepath = m_storageDir + "/" + entry->d_name;
                    struct stat st;
                    if (stat(filepath.c_str(), &st) == 0) {
                        Protocol::FileInfo info;
                        info.filename = entry->d_name;
                        info.fileSize = st.st_size;
                        info.timestamp = st.st_mtime;
                        files.push_back(info);
                    }
                }
            }
            closedir(dir);
        }
#endif
        
        return files;
    }
    
    bool sendMessage(Socket* clientSocket, uint8_t messageType, const std::vector<uint8_t>& payload) {
        Protocol::MessageHeader header(messageType, static_cast<uint32_t>(payload.size()));
        
        uint8_t headerBuffer[8];
        if (!ProtocolHelper::serializeHeader(header, headerBuffer, sizeof(headerBuffer))) {
            return false;
        }
        
        // Send header
        if (clientSocket->send(headerBuffer, sizeof(headerBuffer)) != sizeof(headerBuffer)) {
            return false;
        }
        
        // Send payload if present
        if (!payload.empty()) {
            if (clientSocket->send(payload.data(), payload.size()) != static_cast<int>(payload.size())) {
                return false;
            }
        }
        
        return true;
    }
    
    void sendErrorResponse(Socket* clientSocket, const std::string& errorMsg) {
        auto payload = ProtocolHelper::createStatusPayload(Protocol::STATUS_ERROR, errorMsg);
        sendMessage(clientSocket, Protocol::MSG_ERROR_RESPONSE, payload);
    }
    
    Socket m_serverSocket;
    uint16_t m_port;
    std::string m_storageDir;
    bool m_running;
    
    // Upload state
    std::ofstream m_uploadFile;
    std::string m_uploadFilename;
    uint64_t m_uploadExpectedSize;
    uint64_t m_uploadReceivedSize;
};

int main(int argc, char* argv[]) {
    if (!PlatformUtils::initialize()) {
        std::cerr << "Failed to initialize platform" << std::endl;
        return 1;
    }
    
    uint16_t port = 8080;
    std::string storageDir = "server_files";
    
    if (argc >= 2) {
        port = static_cast<uint16_t>(std::atoi(argv[1]));
    }
    if (argc >= 3) {
        storageDir = argv[2];
    }
    
    FileServer server(port, storageDir);
    
    if (!server.start()) {
        PlatformUtils::cleanup();
        return 1;
    }
    
    server.run();
    
    PlatformUtils::cleanup();
    return 0;
}