#ifndef FILE_UTIL_H
#define FILE_UTIL_H

// Library Header Files
#include <filesystem>
#include <string>

using namespace std;
namespace fs = std::filesystem;

// Filesystem operations shared by repository and object storage.
class FileUtil {
public:
    static constexpr size_t maxSize = 64 * 1024 * 1024;
    static string read(const fs::path& path);
    static void write(const fs::path& path, const string& data);
    static void atomicWrite(const fs::path& path, const string& data);
    static bool exists(const fs::path& path);
    static void directory(const fs::path& path);
    static bool safeName(const string& name);
    static bool safePath(const string& path);
    static bool executable(const fs::path& path);
    static void setMode(const fs::path& path, const string& mode);
};

// Exclusive repository writer lock; readers also lock for a consistent view.
class RepoLock {
private:
    fs::path path;
    int fd = -1;
public:
    explicit RepoLock(const fs::path& path);
    ~RepoLock();
    RepoLock(const RepoLock&) = delete;
    RepoLock& operator=(const RepoLock&) = delete;
};

#endif
