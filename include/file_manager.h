#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include "platform_wrapper.h"
#include "protocol.h"
#include <string>
#include <vector>
#include <fstream>
#include <sys/stat.h>

#ifdef _WIN32
    #include <direct.h>
    #define mkdir _mkdir
    #define stat _stat
#else
    #include <dirent.h>
    #include <sys/types.h>
#endif

class FileManager {
public:
    FileManager(const std::string& storageDir);
    ~FileManager();
    
    std::vector<Protocol::FileInfo> getFileList();
    bool fileExists(const std::string& filename);
    bool deleteFile(const std::string& filename);
    
    std::string getFilePath(const std::string& filename) const;
    bool openForReading(const std::string& filename, std::ifstream& file);
    bool openForWriting(const std::string& filename, std::ofstream& file);
    
    std::string getStorageDir() const { return m_storageDir; }
    
private:
    void createStorageDirectory();
    
    std::string m_storageDir;
    Mutex m_mutex;
};

#endif