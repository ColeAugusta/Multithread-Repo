#include "../include/network_client.h"
#include <QFileInfo>
#include <QFile>
#include <cstring>

// Qt client API implementation
// uses same functions as client.cpp but with QObjects


NetworkClient::NetworkClient(QObject *parent)
    : QObject(parent), m_connected(false) {
}

NetworkClient::~NetworkClient() {
    disconnect();
}

bool NetworkClient::connectToServer(const QString& host, uint16_t port, const QString& password) {
    if (m_connected) {
        emit error("Already connected");
        return false;
    }
    
    if (!m_socket.create()) {
        emit error(QString("Failed to create socket: %1")
                  .arg(QString::fromStdString(m_socket.getLastError())));
        return false;
    }
    
    if (!m_socket.connect(host.toStdString(), port)) {
        emit error(QString("Failed to connect: %1")
                  .arg(QString::fromStdString(m_socket.getLastError())));
        return false;
    }
    
    // hash password
    std::string passwordHash = SecurityHelper::hashPassword(password.toStdString());
    auto payload = ProtocolHelper::createTextPayload(passwordHash);
    
    if (!sendMessage(Protocol::MSG_CONNECT_REQUEST, payload)) {
        emit error("Failed to send authentication");
        m_socket.close();
        return false;
    }
    
    Protocol::MessageHeader header;
    std::vector<uint8_t> responsePayload;
    if (!receiveMessage(header, responsePayload)) {
        emit error("Failed to receive authentication response");
        m_socket.close();
        return false;
    }
    
    if (header.messageType == Protocol::MSG_CONNECT_RESPONSE) {
        m_connected = true;
        emit connected();
        return true;
    } else if (header.messageType == Protocol::MSG_ERROR_RESPONSE) {
        emit error("Authentication failed - incorrect password");
        m_socket.close();
        return false;
    }
    
    emit error("Invalid server response");
    m_socket.close();
    return false;
}

void NetworkClient::disconnect() {
    if (m_connected) {
        sendMessage(Protocol::MSG_DISCONNECT, {});
        m_socket.close();
        m_connected = false;
        emit disconnected();
    }
}


void NetworkClient::refreshFileList() {
    if (!m_connected) {
        emit error("Not connected to server");
        return;
    }
    
    if (!sendMessage(Protocol::MSG_LIST_FILES, {})) {
        emit error("Failed to send list request");
        return;
    }
    
    Protocol::MessageHeader header;
    std::vector<uint8_t> payload;
    if (!receiveMessage(header, payload)) {
        emit error("Failed to receive file list");
        return;
    }
    
    if (header.messageType != Protocol::MSG_FILE_LIST_RESPONSE) {
        emit error("Unexpected response from server");
        return;
    }
    
    uint32_t netFileCount;
    std::memcpy(&netFileCount, payload.data(), sizeof(uint32_t));
    uint32_t fileCount = ntohl(netFileCount);
    
    QStringList files;
    size_t offset = sizeof(uint32_t);
    
    for (uint32_t i = 0; i < fileCount; i++) {
        Protocol::FileInfo fileInfo;
        size_t bytesRead;
        if (ProtocolHelper::deserializeFileInfo(payload.data() + offset, payload.size() - offset, fileInfo, bytesRead)) {
            QString fileStr = QString("%1 (%2 bytes)")
                .arg(QString::fromStdString(fileInfo.filename))
                .arg(fileInfo.fileSize);
            files.append(fileStr);
            offset += bytesRead;
        }
    }
    
    emit fileListReceived(files);
}


void NetworkClient::uploadFile(const QString& localPath) {
    if (!m_connected) {
        emit error("Not connected to server");
        return;
    }
    
    QFile file(localPath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit error(QString("Failed to open file: %1").arg(localPath));
        return;
    }
    
    qint64 fileSize = file.size();
    QFileInfo fileInfo(localPath);
    QString filename = fileInfo.fileName();
    
    std::string filenameStd = filename.toStdString();
    size_t payloadSize = sizeof(uint32_t) + filenameStd.length() + sizeof(uint64_t);
    std::vector<uint8_t> payload(payloadSize);
    
    size_t offset = 0;
    offset += ProtocolHelper::serializeString(filenameStd, payload.data() + offset, payloadSize - offset);
    ProtocolHelper::serializeUint64(fileSize, payload.data() + offset);
    
    if (!sendMessage(Protocol::MSG_UPLOAD_REQUEST, payload)) {
        emit error("Failed to send upload request");
        file.close();
        return;
    }
    
    // waits for OK response
    Protocol::MessageHeader header;
    std::vector<uint8_t> responsePayload;
    if (!receiveMessage(header, responsePayload)) {
        emit error("Failed to receive upload response");
        file.close();
        return;
    }
    
    if (responsePayload.empty() || responsePayload[0] != Protocol::STATUS_OK) {
        emit error("Server rejected upload");
        file.close();
        return;
    }
    
    // send file data in chunks
    const int CHUNK_SIZE = 4096;
    QByteArray buffer;
    qint64 totalSent = 0;
    
    while (!file.atEnd()) {
        buffer = file.read(CHUNK_SIZE);
        std::vector<uint8_t> chunk(buffer.begin(), buffer.end());
        
        if (!sendMessage(Protocol::MSG_UPLOAD_DATA, chunk)) {
            emit error("Failed to send file chunk");
            file.close();
            return;
        }
        
        totalSent += buffer.size();
        int percent = (totalSent * 100) / fileSize;
        emit transferProgress(percent);
    }
    
    file.close();
    
    sendMessage(Protocol::MSG_UPLOAD_COMPLETE, {});
    
    emit transferProgress(100);
    emit transferComplete(QString("Upload complete: %1").arg(filename));
}


void NetworkClient::downloadFile(const QString& remoteFilename, const QString& savePath) {
    if (!m_connected) {
        emit error("Not connected to server");
        return;
    }
    
    // Send download request
    auto payload = ProtocolHelper::createTextPayload(remoteFilename.toStdString());
    if (!sendMessage(Protocol::MSG_DOWNLOAD_REQUEST, payload)) {
        emit error("Failed to send download request");
        return;
    }
    
    // Open file for writing
    QFile file(savePath);
    if (!file.open(QIODevice::WriteOnly)) {
        emit error(QString("Failed to create file: %1").arg(savePath));
        return;
    }
    
    // receive file chunks
    qint64 totalReceived = 0;
    qint64 estimatedSize = 1; // will be updated
    
    while (true) {
        Protocol::MessageHeader header;
        std::vector<uint8_t> chunkPayload;
        
        if (!receiveMessage(header, chunkPayload)) {
            emit error("Failed to receive chunk");
            file.close();
            return;
        }
        
        if (header.messageType == Protocol::MSG_DOWNLOAD_COMPLETE) {
            break;
        }
        
        if (header.messageType == Protocol::MSG_ERROR_RESPONSE) {
            emit error("Server error during download");
            file.close();
            return;
        }
        
        if (header.messageType == Protocol::MSG_DOWNLOAD_DATA) {
            file.write(reinterpret_cast<char*>(chunkPayload.data()), chunkPayload.size());
            totalReceived += chunkPayload.size();
            
            if (totalReceived > estimatedSize) {
                estimatedSize = totalReceived * 2;
            }
            int percent = std::min(99, (int)((totalReceived * 100) / estimatedSize));
            emit transferProgress(percent);
        }
    }
    
    file.close();
    
    emit transferProgress(100);
    emit transferComplete(QString("Download complete: %1 (%2 bytes)").arg(remoteFilename).arg(totalReceived));
}


void NetworkClient::deleteFile(const QString& filename) {
    if (!m_connected) {
        emit error("Not connected to server");
        return;
    }
    
    auto payload = ProtocolHelper::createTextPayload(filename.toStdString());
    if (!sendMessage(Protocol::MSG_DELETE_REQUEST, payload)) {
        emit error("Failed to send delete request");
        return;
    }
    
    Protocol::MessageHeader header;
    std::vector<uint8_t> responsePayload;
    if (receiveMessage(header, responsePayload)) {
        if (header.messageType == Protocol::MSG_DELETE_RESPONSE) {
            if (!responsePayload.empty() && responsePayload[0] == Protocol::STATUS_OK) {
                emit transferComplete(QString("File deleted: %1").arg(filename));
            } else {
                emit error("Failed to delete file");
            }
        }
    }
}


bool NetworkClient::sendMessage(uint8_t messageType, const std::vector<uint8_t>& payload) {
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


bool NetworkClient::receiveMessage(Protocol::MessageHeader& header, std::vector<uint8_t>& payload) {
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