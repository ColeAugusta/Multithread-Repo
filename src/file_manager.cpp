#include "../include/file_manager.h"
#include <ctime>
#include <iostream>


// System implementation to handle files on server per client
// needed for mutex and concurrency

FileManager::FileManager(const std::string& storageDir)
    : m_storageDir(storageDir) {
    createStorageDirectory();
}

FileManager::~FileManager() {
}

void FileManager::createStorageDirectory() {
#ifdef _WIN32
    mkdir(m_storageDir.c_str());
#else
    mkdir(m_storageDir.c_str(), 0755);
#endif
}

std::vector<Protocol::FileInfo> FileManager::getFileList() {
    LockGuard lock(m_mutex);
    std::vector<Protocol::FileInfo> files;
    
    // idk man i think this win32 protocol works
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

bool FileManager::fileExists(const std::string& filename) {
    LockGuard lock(m_mutex);
    std::string filepath = m_storageDir + "/" + filename;
    
    struct stat buffer;
    return (stat(filepath.c_str(), &buffer) == 0);
}

bool FileManager::deleteFile(const std::string& filename) {
    LockGuard lock(m_mutex);
    std::string filepath = m_storageDir + "/" + filename;
    
    return (std::remove(filepath.c_str()) == 0);
}

std::string FileManager::getFilePath(const std::string& filename) const {
    return m_storageDir + "/" + filename;
}

bool FileManager::openForReading(const std::string& filename, std::ifstream& file) {
    LockGuard lock(m_mutex);
    std::string filepath = m_storageDir + "/" + filename;
    file.open(filepath, std::ios::binary);
    return file.is_open();
}

bool FileManager::openForWriting(const std::string& filename, std::ofstream& file) {
    LockGuard lock(m_mutex);
    std::string filepath = m_storageDir + "/" + filename;
    file.open(filepath, std::ios::binary);
    return file.is_open();
}