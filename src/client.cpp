#include "../include/platform_wrapper.h"
#include "../include/protocol.h"
#include <iostream>
#include <fstream>
#include <vector>


// Command-line client API implementation

class SimpleClient {
public:
    SimpleClient() : m_connected(false) {}
    
    bool connect(const std::string& host, uint16_t port, const std::string& passwordHash) {
        if (!m_socket.create()) {
            std::cerr << "Failed to create socket: " << m_socket.getLastError() << std::endl;
            return false;
        }
        
        std::cout << "Connecting to " << host << ":" << port << "..." << std::endl;
        
        if (!m_socket.connect(host, port)) {
            std::cerr << "Failed to connect: " << m_socket.getLastError() << std::endl;
            return false;
        }
        
        std::cout << "Connected!" << std::endl;
        m_connected = true;
        
        auto payload = ProtocolHelper::createTextPayload(passwordHash);
        if (!sendMessage(Protocol::MSG_CONNECT_REQUEST, payload)) {
            return false;
        }
        
        Protocol::MessageHeader header;
        std::vector<uint8_t> responsePayload;
        if (!receiveMessage(header, responsePayload)) {
            return false;
        }
        
        if (header.messageType == Protocol::MSG_CONNECT_RESPONSE) {
            std::string welcomeMsg;
            size_t bytesRead;
            ProtocolHelper::deserializeString(responsePayload.data(), responsePayload.size(), 
                                            welcomeMsg, bytesRead);
            std::cout << "Server: " << welcomeMsg << std::endl;
            return true;
        } else if (header.messageType == Protocol::MSG_ERROR_RESPONSE) {
            std::cerr << "Authentication failed!" << std::endl;
            return false;
        }
        
        return false;
    }
    

    void disconnect() {
        if (m_connected) {
            sendMessage(Protocol::MSG_DISCONNECT, {});
            m_socket.close();
            m_connected = false;
            std::cout << "Disconnected" << std::endl;
        }
    }
    

    void listFiles() {
        if (!m_connected) {
            std::cerr << "Not connected" << std::endl;
            return;
        }
        
        std::cout << "\nRequesting file list..." << std::endl;
        
        if (!sendMessage(Protocol::MSG_LIST_FILES, {})) {
            std::cerr << "Failed to send list request" << std::endl;
            return;
        }
        
        Protocol::MessageHeader header;
        std::vector<uint8_t> payload;
        if (!receiveMessage(header, payload)) {
            std::cerr << "Failed to receive file list" << std::endl;
            return;
        }
        
        if (header.messageType != Protocol::MSG_FILE_LIST_RESPONSE) {
            std::cerr << "Unexpected response" << std::endl;
            return;
        }
        
        uint32_t netFileCount;
        std::memcpy(&netFileCount, payload.data(), sizeof(uint32_t));
        uint32_t fileCount = ntohl(netFileCount);
        
        std::cout << "\nFiles on server (" << fileCount << "):" << std::endl;
        std::cout << "----------------------------------------" << std::endl;
        
        size_t offset = sizeof(uint32_t);
        for (uint32_t i = 0; i < fileCount; i++) {
            Protocol::FileInfo fileInfo;
            size_t bytesRead;
            if (ProtocolHelper::deserializeFileInfo(payload.data() + offset, payload.size() - offset, fileInfo, bytesRead)) {
                std::cout << fileInfo.filename << " (" << fileInfo.fileSize << " bytes)" << std::endl;
                offset += bytesRead;
            }
        }
        std::cout << "----------------------------------------" << std::endl;
    }
    
    
    void uploadFile(const std::string& filepath) {
        if (!m_connected) {
            std::cerr << "Not connected" << std::endl;
            return;
        }
        
        std::ifstream file(filepath, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            std::cerr << "Failed to open file: " << filepath << std::endl;
            return;
        }
        
        std::streamsize fileSize = file.tellg();
        file.seekg(0, std::ios::beg);
        
        std::string filename = filepath;
        size_t lastSlash = filepath.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            filename = filepath.substr(lastSlash + 1);
        }
        
        std::cout << "\nUploading: " << filename << " (" << fileSize << " bytes)" << std::endl;
        
        size_t payloadSize = sizeof(uint32_t) + filename.length() + sizeof(uint64_t);
        std::vector<uint8_t> payload(payloadSize);
        
        size_t offset = 0;
        offset += ProtocolHelper::serializeString(filename, payload.data() + offset, payloadSize - offset);
        ProtocolHelper::serializeUint64(fileSize, payload.data() + offset);
        
        if (!sendMessage(Protocol::MSG_UPLOAD_REQUEST, payload)) {
            std::cerr << "Failed to send upload request" << std::endl;
            return;
        }
        
        // waits for OK response
        Protocol::MessageHeader header;
        std::vector<uint8_t> responsePayload;
        if (!receiveMessage(header, responsePayload)) {
            std::cerr << "Failed to receive response" << std::endl;
            return;
        }
        
        if (responsePayload[0] != Protocol::STATUS_OK) {
            std::cerr << "Server rejected upload" << std::endl;
            return;
        }
        
        // send file data in chunks
        const size_t CHUNK_SIZE = 4096;
        std::vector<uint8_t> buffer(CHUNK_SIZE);
        size_t totalSent = 0;
        
        while (totalSent < fileSize) {
            size_t toRead = std::min(CHUNK_SIZE, static_cast<size_t>(fileSize - totalSent));
            file.read(reinterpret_cast<char*>(buffer.data()), toRead);
            
            std::vector<uint8_t> chunk(buffer.begin(), buffer.begin() + toRead);
            if (!sendMessage(Protocol::MSG_UPLOAD_DATA, chunk)) {
                std::cerr << "Failed to send chunk" << std::endl;
                return;
            }
            
            totalSent += toRead;
            
            int progress = static_cast<int>((totalSent * 100) / fileSize);
            std::cout << "\rProgress: " << progress << "%" << std::flush;
        }
        
        file.close();
        
        sendMessage(Protocol::MSG_UPLOAD_COMPLETE, {});
        
        std::cout << "\nUpload complete!" << std::endl;
    }
    
    void downloadFile(const std::string& filename, const std::string& savePath) {
        if (!m_connected) {
            std::cerr << "Not connected" << std::endl;
            return;
        }
        
        std::cout << "\nDownloading: " << filename << std::endl;
        
        auto payload = ProtocolHelper::createTextPayload(filename);
        if (!sendMessage(Protocol::MSG_DOWNLOAD_REQUEST, payload)) {
            std::cerr << "Failed to send download request" << std::endl;
            return;
        }
        
        std::ofstream file(savePath, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Failed to create file: " << savePath << std::endl;
            return;
        }
        
        // receive file in chunks
        size_t totalReceived = 0;
        while (true) {
            Protocol::MessageHeader header;
            std::vector<uint8_t> chunkPayload;
            
            if (!receiveMessage(header, chunkPayload)) {
                std::cerr << "Failed to receive chunk" << std::endl;
                break;
            }
            
            if (header.messageType == Protocol::MSG_DOWNLOAD_COMPLETE) {
                std::cout << "\nDownload complete! Saved to: " << savePath << std::endl;
                break;
            }
            
            if (header.messageType == Protocol::MSG_ERROR_RESPONSE) {
                std::cerr << "Server error during download" << std::endl;
                break;
            }
            
            if (header.messageType == Protocol::MSG_DOWNLOAD_DATA) {
                file.write(reinterpret_cast<char*>(chunkPayload.data()), chunkPayload.size());
                totalReceived += chunkPayload.size();
                std::cout << "\rReceived: " << totalReceived << " bytes" << std::flush;
            }
        }
        
        file.close();
    }
    

    void deleteFile(const std::string& filename) {
        if (!m_connected) {
            std::cerr << "Not connected" << std::endl;
            return;
        }
        
        std::cout << "\nDeleting: " << filename << std::endl;
        
        auto payload = ProtocolHelper::createTextPayload(filename);
        if (!sendMessage(Protocol::MSG_DELETE_REQUEST, payload)) {
            std::cerr << "Failed to send delete request" << std::endl;
            return;
        }
        
        Protocol::MessageHeader header;
        std::vector<uint8_t> responsePayload;
        if (receiveMessage(header, responsePayload)) {
            if (header.messageType == Protocol::MSG_DELETE_RESPONSE) {
                if (responsePayload[0] == Protocol::STATUS_OK) {
                    std::cout << "File deleted successfully" << std::endl;
                } else {
                    std::cout << "Failed to delete file" << std::endl;
                }
            }
        }
    }
    
    // send and receive implementations just client side
private:
    bool sendMessage(uint8_t messageType, const std::vector<uint8_t>& payload) {
        Protocol::MessageHeader header(messageType, static_cast<uint32_t>(payload.size()));
        
        uint8_t headerBuffer[8];
        if (!ProtocolHelper::serializeHeader(header, headerBuffer, sizeof(headerBuffer))) {
            return false;
        }
        
        if (m_socket.send(headerBuffer, sizeof(headerBuffer)) != sizeof(headerBuffer)) {
            return false;
        }
        
        if (!payload.empty()) {
            if (m_socket.send(payload.data(), payload.size()) != static_cast<int>(payload.size())) {
                return false;
            }
        }
        
        return true;
    }
    
    bool receiveMessage(Protocol::MessageHeader& header, std::vector<uint8_t>& payload) {
        uint8_t headerBuffer[8];
        
        int received = m_socket.receive(headerBuffer, sizeof(headerBuffer));
        if (received != sizeof(headerBuffer)) {
            return false;
        }
        
        if (!ProtocolHelper::deserializeHeader(headerBuffer, sizeof(headerBuffer), header)) {
            return false;
        }
        
        if (header.payloadLength > 0) {
            payload.resize(header.payloadLength);
            int totalReceived = 0;
            while (totalReceived < header.payloadLength) {
                int r = m_socket.receive(payload.data() + totalReceived, 
                                        header.payloadLength - totalReceived);
                if (r <= 0) return false;
                totalReceived += r;
            }
        }
        
        return true;
    }
    
    Socket m_socket;
    bool m_connected;
};

void printUsage(const char* progName) {
    std::cout << "Usage:" << std::endl;
    std::cout << "  " << progName << " <host> <port> <command> [args]" << std::endl;
    std::cout << "\nCommands:" << std::endl;
    std::cout << "  list                    - List files on server" << std::endl;
    std::cout << "  upload <filepath>       - Upload file to server" << std::endl;
    std::cout << "  download <filename> <savepath> - Download file from server" << std::endl;
    std::cout << "  delete <filename>       - Delete file from server" << std::endl;
}

int main(int argc, char* argv[]) {
    if (!PlatformUtils::initialize()) {
        std::cerr << "Failed to initialize platform" << std::endl;
        return 1;
    }
    
    if (argc < 4) {
        printUsage(argv[0]);
        PlatformUtils::cleanup();
        return 1;
    }
    
    std::string host = argv[1];
    uint16_t port = static_cast<uint16_t>(std::atoi(argv[2]));
    std::string command = argv[3];
    
    std::string password;
    std::cout << "Enter password: ";
    std::getline(std::cin, password);
    std::string passwordHash = SecurityHelper::hashPassword(password);

    SimpleClient client;
    
    if (!client.connect(host, port, passwordHash)) {
        PlatformUtils::cleanup();
        return 1;
    }
    
    if (command == "list") {
        client.listFiles();
    } else if (command == "upload" && argc >= 5) {
        client.uploadFile(argv[4]);
    } else if (command == "download" && argc >= 6) {
        client.downloadFile(argv[4], argv[5]);
    } else if (command == "delete" && argc >= 5) {
        client.deleteFile(argv[4]);
    } else {
        std::cerr << "Invalid command or missing arguments" << std::endl;
        printUsage(argv[0]);
    }
    
    client.disconnect();
    
    PlatformUtils::cleanup();
    return 0;
}