// Library Header Files
#include <cstdint>
#include <stdexcept>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cerrno>
#include <cstring>

// Imported Files
#include "../include/fileUtil.h"

using namespace std;

bool FileUtil::exists(const fs::path& path) {
    return fs::symlink_status(path).type() != fs::file_type::not_found;
}

void FileUtil::directory(const fs::path& path) {
    if (!fs::is_directory(fs::symlink_status(path)))
        throw runtime_error("Expected a real directory: " + path.string());
}

// O_NOFOLLOW prevents a file symlink from redirecting metadata or file reads.
string FileUtil::read(const fs::path& path) {
    int fd = open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
    if (fd < 0)
        throw runtime_error("Cannot read " + path.string() + ": " + strerror(errno));
    struct stat info{};
    if (fstat(fd, &info) < 0 || !S_ISREG(info.st_mode) || info.st_size < 0 ||
        static_cast<uint64_t>(info.st_size) > maxSize) {
        close(fd);
        throw runtime_error("Not a regular file or exceeds 64 MiB: " + path.string());
    }
    string data;
    char buf[65536];
    for (;;) {
        ssize_t count = ::read(fd, buf, sizeof(buf));
        if (count < 0 && errno == EINTR)
            continue;
        if (count < 0 || (count > 0 && data.size() + size_t(count) > maxSize)) {
            close(fd);
            throw runtime_error("Read failed or file grew beyond limit: " + path.string());
        }
        if (count == 0)
            break;
        data.append(buf, size_t(count));
    }
    struct stat after{};
    bool changed = fstat(fd, &after) < 0 || after.st_size != info.st_size ||
        after.st_mtim.tv_sec != info.st_mtim.tv_sec || after.st_mtim.tv_nsec != info.st_mtim.tv_nsec ||
        after.st_ctim.tv_sec != info.st_ctim.tv_sec || after.st_ctim.tv_nsec != info.st_ctim.tv_nsec;
    close(fd);
    if (changed || data.size() != size_t(info.st_size))
        throw runtime_error("File changed while reading: " + path.string());
    return data;
}

void FileUtil::write(const fs::path& path, const string& data) {
    int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
    if (fd < 0)
        throw runtime_error("Cannot create " + path.string() + ": " + strerror(errno));
    size_t pos = 0;
    bool ok = true;
    while (pos < data.size()) {
        ssize_t count = ::write(fd, data.data() + pos, data.size() - pos);
        if (count < 0 && errno == EINTR)
            continue;
        if (count <= 0) {
            ok = false;
            break;
        }
        pos += size_t(count);
    }
    if (ok && fsync(fd) != 0)
        ok = false;
    if (close(fd) != 0)
        ok = false;
    if (!ok) {
        fs::remove(path);
        throw runtime_error("Cannot finish writing: " + path.string());
    }
}

void FileUtil::atomicWrite(const fs::path& path, const string& data) {
    directory(path.parent_path());
    if (exists(path) && !fs::is_regular_file(fs::symlink_status(path)))
        throw runtime_error("Invalid metadata file: " + path.string());
    fs::path temp = path.string() + ".tmp";
    // Never erase another operation's temporary file.
    write(temp, data);
    try {
        fs::rename(temp, path);
    }
    catch (...) {
        fs::remove(temp);
        throw;
    }
}

bool FileUtil::safeName(const string& name) {
    if (name.empty() || name == "." || name == ".." || name == ".mygit" || name == ".git")
        return false;
    for (unsigned char ch : name) {
        if (ch == '/' || ch == '\\' || ch < 32 || ch == 127)
            return false;
    }
    return true;
}

bool FileUtil::safePath(const string& path) {
    fs::path value(path);
    if (path.empty() || value.is_absolute() || value.generic_string() != path)
        return false;
    for (const auto& part : value) {
        if (!safeName(part.string()))
            return false;
    }
    return true;
}

bool FileUtil::executable(const fs::path& path) {
    return (fs::status(path).permissions() & fs::perms::owner_exec) != fs::perms::none;
}

void FileUtil::setMode(const fs::path& path, const string& mode) {
    fs::permissions(path, mode == "100755" ? fs::perms(0755) : fs::perms(0644));
}

RepoLock::RepoLock(const fs::path& value) : path(value) {
    fd = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
    if (fd < 0)
        throw runtime_error("Repository is locked or lock cannot be created. "
                            "If no mygit process is running, remove .mygit/LOCK.");
}

RepoLock::~RepoLock() {
    if (fd >= 0) {
        close(fd);
        std::error_code error;
        fs::remove(path, error);
    }
}
