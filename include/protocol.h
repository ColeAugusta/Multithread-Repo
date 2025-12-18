#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstdint>
#include <string>
#include <vector>
#include <cstring>

// Protocol implementation

namespace Protocol {
    const uint16_t MAGIC_NUMBER = 0x4653;  // "FS" for File Sharing
    const uint8_t PROTOCOL_VERSION = 1;
    const size_t MAX_FILENAME_LENGTH = 255;
    const uint64_t MAX_FILE_SIZE = 1024ULL * 1024ULL * 1024ULL; // 1GB
    const size_t MAX_PASSWORD_LENGTH = 128;
    const int CONNECTION_TIMEOUT_SECONDS = 300; // 5 minutes
    
    enum MessageType : uint8_t {
        MSG_CONNECT_REQUEST = 0x01,
        MSG_CONNECT_RESPONSE = 0x02,
        MSG_LIST_FILES = 0x03,
        MSG_FILE_LIST_RESPONSE = 0x04,
        MSG_UPLOAD_REQUEST = 0x05,
        MSG_UPLOAD_DATA = 0x06,
        MSG_UPLOAD_COMPLETE = 0x07,
        MSG_DOWNLOAD_REQUEST = 0x08,
        MSG_DOWNLOAD_DATA = 0x09,
        MSG_DOWNLOAD_COMPLETE = 0x0A,
        MSG_DELETE_REQUEST = 0x0B,
        MSG_DELETE_RESPONSE = 0x0C,
        MSG_ERROR_RESPONSE = 0xFE,
        MSG_DISCONNECT = 0xFF
    };
    
    enum StatusCode : uint8_t {
        STATUS_OK = 0x00,
        STATUS_ERROR = 0x01,
        STATUS_FILE_NOT_FOUND = 0x02,
        STATUS_ACCESS_DENIED = 0x03,
        STATUS_INVALID_REQUEST = 0x04,
        STATUS_FILE_EXISTS = 0x05
    };
    
    // fixed 8 bytes header structure
    struct MessageHeader {
        uint16_t magicNumber;      // 0x4653
        uint8_t version;           // Protocol version
        uint8_t messageType;       // MessageType enum
        uint32_t payloadLength;    // Length of payload data
        
        MessageHeader() 
            : magicNumber(MAGIC_NUMBER), version(PROTOCOL_VERSION), 
              messageType(0), payloadLength(0) {}
        
        MessageHeader(uint8_t type, uint32_t length)
            : magicNumber(MAGIC_NUMBER), version(PROTOCOL_VERSION),
              messageType(type), payloadLength(length) {}
    };
    
    struct FileInfo {
        std::string filename;
        uint64_t fileSize;
        uint64_t timestamp;
        
        FileInfo() : fileSize(0), timestamp(0) {}
        FileInfo(const std::string& name, uint64_t size, uint64_t time)
            : filename(name), fileSize(size), timestamp(time) {}
    };
};

// Protocol message serialization/deserialization helper
class ProtocolHelper {
public:
    static bool serializeHeader(const Protocol::MessageHeader& header, uint8_t* buffer, size_t bufferSize) {
        if (bufferSize < sizeof(Protocol::MessageHeader)) return false;
        
        // network byte order
        uint16_t magic = htons(header.magicNumber);
        uint32_t length = htonl(header.payloadLength);
        
        std::memcpy(buffer, &magic, sizeof(uint16_t));
        buffer[2] = header.version;
        buffer[3] = header.messageType;
        std::memcpy(buffer + 4, &length, sizeof(uint32_t));
        
        return true;
    }
    
    // deserialize header from buffer
    static bool deserializeHeader(const uint8_t* buffer, size_t bufferSize, Protocol::MessageHeader& header) {
        if (bufferSize < sizeof(Protocol::MessageHeader)) return false;
        
        uint16_t magic;
        uint32_t length;
        
        std::memcpy(&magic, buffer, sizeof(uint16_t));
        header.magicNumber = ntohs(magic);
        header.version = buffer[2];
        header.messageType = buffer[3];
        std::memcpy(&length, buffer + 4, sizeof(uint32_t));
        header.payloadLength = ntohl(length);
        
        // magic number needed
        if (header.magicNumber != Protocol::MAGIC_NUMBER) {
            return false;
        }
        
        return true;
    }
    
    // string -> buffer with length prefix
    static size_t serializeString(const std::string& str, uint8_t* buffer, size_t bufferSize) {
        uint32_t length = static_cast<uint32_t>(str.length());
        size_t totalSize = sizeof(uint32_t) + length;
        
        if (bufferSize < totalSize) return 0;
        
        uint32_t netLength = htonl(length);
        std::memcpy(buffer, &netLength, sizeof(uint32_t));
        std::memcpy(buffer + sizeof(uint32_t), str.c_str(), length);
        
        return totalSize;
    }
    
    // deserialize string from buffer
    static bool deserializeString(const uint8_t* buffer, size_t bufferSize, std::string& str, size_t& bytesRead) {
        if (bufferSize < sizeof(uint32_t)) return false;
        
        uint32_t netLength;
        std::memcpy(&netLength, buffer, sizeof(uint32_t));
        uint32_t length = ntohl(netLength);
        
        if (bufferSize < sizeof(uint32_t) + length) return false;
        
        str.assign(reinterpret_cast<const char*>(buffer + sizeof(uint32_t)), length);
        bytesRead = sizeof(uint32_t) + length;
        
        return true;
    }
    
    // uint64_t to buffer
    static void serializeUint64(uint64_t value, uint8_t* buffer) {
        // Convert to network byte order (big-endian)
        for (int i = 7; i >= 0; i--) {
            buffer[7 - i] = static_cast<uint8_t>((value >> (i * 8)) & 0xFF);
        }
    }
    
    // deserialize uint64_t from buffer
    static uint64_t deserializeUint64(const uint8_t* buffer) {
        uint64_t value = 0;
        for (int i = 0; i < 8; i++) {
            value |= static_cast<uint64_t>(buffer[i]) << ((7 - i) * 8);
        }
        return value;
    }
    
    // FileInfo to buffer
    static size_t serializeFileInfo(const Protocol::FileInfo& fileInfo, uint8_t* buffer, size_t bufferSize) {
        size_t offset = 0;
        
        size_t strSize = serializeString(fileInfo.filename, buffer + offset, bufferSize - offset);
        if (strSize == 0) return 0;
        offset += strSize;
        
        if (bufferSize - offset < sizeof(uint64_t)) return 0;
        serializeUint64(fileInfo.fileSize, buffer + offset);
        offset += sizeof(uint64_t);
        
        if (bufferSize - offset < sizeof(uint64_t)) return 0;
        serializeUint64(fileInfo.timestamp, buffer + offset);
        offset += sizeof(uint64_t);
        
        return offset;
    }
    
    // deserialize FileInfo from buffer
    static bool deserializeFileInfo(const uint8_t* buffer, size_t bufferSize, Protocol::FileInfo& fileInfo, size_t& bytesRead) {
        size_t offset = 0;
        
        size_t strBytes;
        if (!deserializeString(buffer + offset, bufferSize - offset, fileInfo.filename, strBytes)) {
            return false;
        }
        offset += strBytes;
        
        if (bufferSize - offset < sizeof(uint64_t)) return false;
        fileInfo.fileSize = deserializeUint64(buffer + offset);
        offset += sizeof(uint64_t);
        
        if (bufferSize - offset < sizeof(uint64_t)) return false;
        fileInfo.timestamp = deserializeUint64(buffer + offset);
        offset += sizeof(uint64_t);
        
        bytesRead = offset;
        return true;
    }
    
    static std::vector<uint8_t> createTextPayload(const std::string& text) {
        std::vector<uint8_t> payload(sizeof(uint32_t) + text.length());
        serializeString(text, payload.data(), payload.size());
        return payload;
    }
    
    static std::vector<uint8_t> createStatusPayload(Protocol::StatusCode status, const std::string& message = "") {
        std::vector<uint8_t> payload(1);
        payload[0] = status;
        
        if (!message.empty()) {
            size_t msgSize = sizeof(uint32_t) + message.length();
            payload.resize(1 + msgSize);
            serializeString(message, payload.data() + 1, msgSize);
        }
        
        return payload;
    }
};

// helper to handle authentication along protocol
class SecurityHelper {
public:
    static std::string hashPassword(const std::string& password) {
        // hashes server password to prevent unauthorized access
        size_t hash = 5381;
        for (char c : password) {
            hash = ((hash << 5) + hash) + c;
        }
        char buf[32];
        snprintf(buf, sizeof(buf), "%016zx", hash);
        return std::string(buf);
    }
    
    // validates filename to prevent path traversal attacks
    static bool isValidFilename(const std::string& filename) {
        if (filename.empty() || filename.length() > Protocol::MAX_FILENAME_LENGTH) {
            return false;
        }
        
        // directory traversal attempts
        if (filename.find("..") != std::string::npos) {
            return false;
        }
        if (filename.find("/") != std::string::npos) {
            return false;
        }
        if (filename.find("\\") != std::string::npos) {
            return false;
        }
        
        // null bytes
        if (filename.find('\0') != std::string::npos) {
            return false;
        }
        
        return true;
    }
    
    static bool isValidFileSize(uint64_t size) {
        return size > 0 && size <= Protocol::MAX_FILE_SIZE;
    }
};
#endif