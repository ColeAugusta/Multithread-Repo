#ifndef NETWORK_CLIENT_H
#define NETWORK_CLIENT_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QThread>
#include "platform_wrapper.h"
#include "protocol.h"

class NetworkClient : public QObject {
    Q_OBJECT

public:
    explicit NetworkClient(QObject *parent = nullptr);
    ~NetworkClient();
    
    bool connectToServer(const QString& host, uint16_t port, const QString& password);
    void disconnect();
    bool isConnected() const { return m_connected; }
    
    void refreshFileList();
    void uploadFile(const QString& localPath);
    void downloadFile(const QString& remoteFilename, const QString& savePath);
    void deleteFile(const QString& filename);

signals:
    void connected();
    void disconnected();
    void error(const QString& errorMsg);
    void fileListReceived(const QStringList& files);
    void transferProgress(int percent);
    void transferComplete(const QString& message);

private:
    bool sendMessage(uint8_t messageType, const std::vector<uint8_t>& payload);
    bool receiveMessage(Protocol::MessageHeader& header, std::vector<uint8_t>& payload);
    
    Socket m_socket;
    bool m_connected;
};

#endif