#include "../include/mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>

// Qt GUI structure


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_client(nullptr) {
    
    setWindowTitle("File Sharing Client");
    resize(800, 600);
    
    m_client = new NetworkClient(this);
    
    setupUI();
    
    connect(m_connectButton, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(m_disconnectButton, &QPushButton::clicked, this, &MainWindow::onDisconnectClicked);
    connect(m_refreshButton, &QPushButton::clicked, this, &MainWindow::onRefreshClicked);
    connect(m_uploadButton, &QPushButton::clicked, this, &MainWindow::onUploadClicked);
    connect(m_downloadButton, &QPushButton::clicked, this, &MainWindow::onDownloadClicked);
    connect(m_deleteButton, &QPushButton::clicked, this, &MainWindow::onDeleteClicked);
    
    connect(m_client, &NetworkClient::connected, this, &MainWindow::onConnected);
    connect(m_client, &NetworkClient::disconnected, this, &MainWindow::onDisconnected);
    connect(m_client, &NetworkClient::error, this, &MainWindow::onError);
    connect(m_client, &NetworkClient::fileListReceived, this, &MainWindow::onFileListReceived);
    connect(m_client, &NetworkClient::transferProgress, this, &MainWindow::onTransferProgress);
    connect(m_client, &NetworkClient::transferComplete, this, &MainWindow::onTransferComplete);
    
    setConnectedState(false);
}

MainWindow::~MainWindow() {
}

void MainWindow::setupUI() {
    QWidget* centralWidget = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    
    // connections
    m_connectionGroup = new QGroupBox("Connection", this);
    QHBoxLayout* connLayout = new QHBoxLayout(m_connectionGroup);
    
    connLayout->addWidget(new QLabel("Host:"));
    m_hostEdit = new QLineEdit("localhost", this);
    m_hostEdit->setMaximumWidth(150);
    connLayout->addWidget(m_hostEdit);
    
    connLayout->addWidget(new QLabel("Port:"));
    m_portEdit = new QLineEdit("8080", this);
    m_portEdit->setMaximumWidth(80);
    connLayout->addWidget(m_portEdit);
    
    m_connectButton = new QPushButton("Connect", this);
    connLayout->addWidget(m_connectButton);
    
    m_disconnectButton = new QPushButton("Disconnect", this);
    connLayout->addWidget(m_disconnectButton);

    connLayout->addWidget(new QLabel("Password:"));
    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setMaximumWidth(120);
    connLayout->addWidget(m_passwordEdit);
    
    m_statusLabel = new QLabel("Not Connected", this);
    m_statusLabel->setStyleSheet("QLabel { color: red; font-weight: bold; }");
    connLayout->addWidget(m_statusLabel);
    
    connLayout->addStretch();
    
    mainLayout->addWidget(m_connectionGroup);
    
    // file list
    m_fileListGroup = new QGroupBox("Server Files", this);
    QVBoxLayout* fileListLayout = new QVBoxLayout(m_fileListGroup);
    
    m_fileList = new QListWidget(this);
    fileListLayout->addWidget(m_fileList);
    
    m_refreshButton = new QPushButton("Refresh", this);
    fileListLayout->addWidget(m_refreshButton);
    
    mainLayout->addWidget(m_fileListGroup);
    
    // operations
    m_operationsGroup = new QGroupBox("Operations", this);
    QHBoxLayout* opsLayout = new QHBoxLayout(m_operationsGroup);
    
    m_uploadButton = new QPushButton("Upload File", this);
    opsLayout->addWidget(m_uploadButton);
    
    m_downloadButton = new QPushButton("Download Selected", this);
    opsLayout->addWidget(m_downloadButton);
    
    m_deleteButton = new QPushButton("Delete Selected", this);
    opsLayout->addWidget(m_deleteButton);
    
    opsLayout->addStretch();
    
    mainLayout->addWidget(m_operationsGroup);
    
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    mainLayout->addWidget(m_progressBar);
    
    // logs
    QGroupBox* logGroup = new QGroupBox("Log", this);
    QVBoxLayout* logLayout = new QVBoxLayout(logGroup);
    
    m_logEdit = new QTextEdit(this);
    m_logEdit->setReadOnly(true);
    m_logEdit->setMaximumHeight(150);
    logLayout->addWidget(m_logEdit);
    
    mainLayout->addWidget(logGroup);
    
    setCentralWidget(centralWidget);
}


void MainWindow::onConnectClicked() {
    QString host = m_hostEdit->text();
    bool ok;
    uint16_t port = m_portEdit->text().toUShort(&ok);
    QString password = m_passwordEdit->text();
    
    if (!ok || port == 0) {
        QMessageBox::warning(this, "Invalid Port", "Please enter a valid port number");
        return;
    }

    if (password.isEmpty()) {
        QMessageBox::warning(this, "Password Required", "Please enter the server password");
        return;
    }
    
    log(QString("Connecting to %1:%2...").arg(host).arg(port));
    
    if (m_client->connectToServer(host, port, password)) {
        log("Connection successful!");
    }
}


void MainWindow::onDisconnectClicked() {
    m_client->disconnect();
    log("Disconnected from server");
}


void MainWindow::onRefreshClicked() {
    log("Refreshing file list...");
    m_client->refreshFileList();
}


void MainWindow::onUploadClicked() {
    QString filePath = QFileDialog::getOpenFileName(this, "Select File to Upload");
    if (filePath.isEmpty()) {
        return;
    }
    
    log(QString("Uploading: %1").arg(filePath));
    m_progressBar->setValue(0);
    m_client->uploadFile(filePath);
}


void MainWindow::onDownloadClicked() {
    QListWidgetItem* item = m_fileList->currentItem();
    if (!item) {
        QMessageBox::warning(this, "No Selection", "Please select a file to download");
        return;
    }
    
    QString fullText = item->text();
    QString filename = fullText.split(" (").first();
    
    QString savePath = QFileDialog::getSaveFileName(this, "Save File As", filename);
    if (savePath.isEmpty()) {
        return;
    }
    
    log(QString("Downloading: %1").arg(filename));
    m_progressBar->setValue(0);
    m_client->downloadFile(filename, savePath);
}


void MainWindow::onDeleteClicked() {
    QListWidgetItem* item = m_fileList->currentItem();
    if (!item) {
        QMessageBox::warning(this, "No Selection", "Please select a file to delete");
        return;
    }
    
    QString fullText = item->text();
    QString filename = fullText.split(" (").first();
    
    QMessageBox::StandardButton reply = QMessageBox::question(this, "Confirm Delete",
        QString("Are you sure you want to delete '%1'?").arg(filename),
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        log(QString("Deleting: %1").arg(filename));
        m_client->deleteFile(filename);
        
        // refresh list after delete
        QTimer::singleShot(500, this, &MainWindow::onRefreshClicked);
    }
}


void MainWindow::onConnected() {
    setConnectedState(true);
    log("Connected to server successfully");
    onRefreshClicked();
}


void MainWindow::onDisconnected() {
    setConnectedState(false);
    m_fileList->clear();
}

void MainWindow::onError(const QString& error) {
    log(QString("ERROR: %1").arg(error));
    QMessageBox::critical(this, "Error", error);
}


void MainWindow::onFileListReceived(const QStringList& files) {
    m_fileList->clear();
    m_fileList->addItems(files);
    log(QString("File list updated (%1 files)").arg(files.size()));
}


void MainWindow::onTransferProgress(int percent) {
    m_progressBar->setValue(percent);
}


void MainWindow::onTransferComplete(const QString& message) {
    log(message);
    m_progressBar->setValue(0);
    
    // refresh file list after upload/delete
    QTimer::singleShot(500, this, &MainWindow::onRefreshClicked);
}


void MainWindow::log(const QString& message) {
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    m_logEdit->append(QString("[%1] %2").arg(timestamp).arg(message));
}


void MainWindow::setConnectedState(bool connected) {
    m_connectButton->setEnabled(!connected);
    m_disconnectButton->setEnabled(connected);
    m_hostEdit->setEnabled(!connected);
    m_portEdit->setEnabled(!connected);
    
    m_refreshButton->setEnabled(connected);
    m_uploadButton->setEnabled(connected);
    m_downloadButton->setEnabled(connected);
    m_deleteButton->setEnabled(connected);
    m_fileList->setEnabled(connected);
    
    if (connected) {
        m_statusLabel->setText("Connected");
        m_statusLabel->setStyleSheet("QLabel { color: green; font-weight: bold; }");
    } else {
        m_statusLabel->setText("Not Connected");
        m_statusLabel->setStyleSheet("QLabel { color: red; font-weight: bold; }");
    }
}