#ifndef SD_H
#define SD_H

/**
 * SD.h Mock for LED Grid Simulator
 * 
 * Provides SD card API that reads from the local filesystem.
 */

#include "Arduino.h"
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <string>
#include <vector>

// Forward declare for SPI config
class SPIClass;
extern SPIClass SPI1;

// SdSpiConfig mock
class SdSpiConfig {
public:
    SdSpiConfig(int cs, int shared, int speed, SPIClass* spi = nullptr) {
        (void)cs; (void)shared; (void)speed; (void)spi;
    }
};

// File class that wraps FILE*
class File {
private:
    FILE* _file;
    std::string _name;
    std::string _path;
    bool _isDir;
    DIR* _dir;
    std::string _basePath;  // For directory iteration
    
public:
    File() : _file(nullptr), _isDir(false), _dir(nullptr) {}
    
    File(const std::string& path, const char* mode = "rb") 
        : _file(nullptr), _path(path), _isDir(false), _dir(nullptr) {
        
        // Extract filename from path
        size_t pos = path.rfind('/');
        _name = (pos != std::string::npos) ? path.substr(pos + 1) : path;
        
        struct stat st;
        if (stat(path.c_str(), &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                _isDir = true;
                _dir = opendir(path.c_str());
                _basePath = path;
            } else {
                _file = fopen(path.c_str(), mode);
            }
        }
    }
    
    ~File() {
        close();
    }
    
    // Move constructor
    File(File&& other) noexcept 
        : _file(other._file), _name(std::move(other._name)), 
          _path(std::move(other._path)), _isDir(other._isDir),
          _dir(other._dir), _basePath(std::move(other._basePath)) {
        other._file = nullptr;
        other._dir = nullptr;
    }
    
    // Move assignment
    File& operator=(File&& other) noexcept {
        if (this != &other) {
            close();
            _file = other._file;
            _name = std::move(other._name);
            _path = std::move(other._path);
            _isDir = other._isDir;
            _dir = other._dir;
            _basePath = std::move(other._basePath);
            other._file = nullptr;
            other._dir = nullptr;
        }
        return *this;
    }
    
    // Disable copy
    File(const File&) = delete;
    File& operator=(const File&) = delete;
    
    operator bool() const {
        return _file != nullptr || _dir != nullptr;
    }
    
    const char* name() const {
        return _name.c_str();
    }
    
    uint32_t size() {
        if (!_file) return 0;
        long pos = ftell(_file);
        fseek(_file, 0, SEEK_END);
        long sz = ftell(_file);
        fseek(_file, pos, SEEK_SET);
        return (uint32_t)sz;
    }
    
    int read() {
        if (!_file) return -1;
        int c = fgetc(_file);
        return (c == EOF) ? -1 : c;
    }
    
    int read(uint8_t* buf, size_t size) {
        if (!_file) return 0;
        return fread(buf, 1, size, _file);
    }
    
    bool seek(uint32_t pos) {
        if (!_file) return false;
        return fseek(_file, pos, SEEK_SET) == 0;
    }
    
    uint32_t position() {
        if (!_file) return 0;
        return (uint32_t)ftell(_file);
    }
    
    void close() {
        if (_file) {
            fclose(_file);
            _file = nullptr;
        }
        if (_dir) {
            closedir(_dir);
            _dir = nullptr;
        }
    }
    
    bool isDirectory() {
        return _isDir;
    }
    
    File openNextFile() {
        if (!_dir) return File();
        
        struct dirent* entry;
        while ((entry = readdir(_dir)) != nullptr) {
            // Skip . and ..
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            
            std::string fullPath = _basePath;
            if (!fullPath.empty() && fullPath.back() != '/') {
                fullPath += '/';
            }
            fullPath += entry->d_name;
            
            return File(fullPath);
        }
        
        return File();
    }
};

// SdFs mock for advanced SD operations
class SdFs {
public:
    bool begin(const SdSpiConfig& config) {
        (void)config;
        return true;
    }
};

// Main SD class
class SDClass {
private:
    std::string _basePath;
    bool _initialized;
    
public:
    SdFs sdfs;
    
    SDClass() : _initialized(false) {}
    
    void setBasePath(const std::string& path) {
        _basePath = path;
    }
    
    bool begin(int cs = 0) {
        (void)cs;
        _initialized = true;
        return true;
    }
    
    File open(const char* path, const char* mode = "rb") {
        std::string fullPath;
        
        // If path is absolute, use it directly
        if (path[0] == '/') {
            fullPath = _basePath + path;
        } else {
            fullPath = _basePath + "/" + path;
        }
        
        return File(fullPath, mode);
    }
    
    bool exists(const char* path) {
        std::string fullPath = _basePath + path;
        struct stat st;
        return stat(fullPath.c_str(), &st) == 0;
    }
};

extern SDClass SD;

// Set the base path for SD operations (called from simulator main)
void SD_setBasePath(const std::string& path);

#endif // SD_H
