#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QTextEdit>
#include <QProgressBar>
#include <QGroupBox>
#include <QTimer>
#include "network_client.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onConnectClicked();
    void onDisconnectClicked();
    
    void onRefreshClicked();
    void onUploadClicked();
    void onDownloadClicked();
    void onDeleteClicked();
    
    void onConnected();
    void onDisconnected();
    void onError(const QString& error);
    void onFileListReceived(const QStringList& files);
    void onTransferProgress(int percent);
    void onTransferComplete(const QString& message);

private:
    void setupUI();
    void log(const QString& message);
    void setConnectedState(bool connected);

    
    // UI Components - Connection
    QGroupBox* m_connectionGroup;
    QLineEdit* m_hostEdit;
    QLineEdit* m_portEdit;
    QPushButton* m_connectButton;
    QPushButton* m_disconnectButton;
    QLineEdit* m_passwordEdit;
    QLabel* m_statusLabel;
    
    // UI Components - File List
    QGroupBox* m_fileListGroup;
    QListWidget* m_fileList;
    QPushButton* m_refreshButton;
    
    // UI Components - Operations
    QGroupBox* m_operationsGroup;
    QPushButton* m_uploadButton;
    QPushButton* m_downloadButton;
    QPushButton* m_deleteButton;
    
    // UI Components - Progress
    QProgressBar* m_progressBar;
    
    // UI Components - Log
    QTextEdit* m_logEdit;
    
    // Network client
    NetworkClient* m_client;
};

#endif