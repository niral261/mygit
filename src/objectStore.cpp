// Library Header Files
#include <zlib.h>
#include <stdexcept>

// Imported Files
#include "../include/objectStore.h"
#include "../include/hash.h"

using namespace std;

ObjectStore::ObjectStore(const fs::path& value) : root(value) {
    FileUtil::directory(root);
}

fs::path ObjectStore::objectPath(const string& sha) const {
    if (!Hash::valid(sha))
        throw runtime_error("Expected a complete 40-character lowercase SHA1");
    fs::path dir = root / sha.substr(0, 2);
    if (FileUtil::exists(dir))
        FileUtil::directory(dir);
    return dir / sha.substr(2);
}

string ObjectStore::hash(const string& type, const string& data) {
    return Hash::digest(type + " " + to_string(data.size()) + '\0' + data);
}

string ObjectStore::put(const string& type, const string& data) {
    if (type != "blob" && type != "tree" && type != "commit")
        throw runtime_error("Unsupported object type");
    string raw = type + " " + to_string(data.size()) + '\0' + data;
    if (raw.size() > FileUtil::maxSize)
        throw runtime_error("Object exceeds 64 MiB including header");
    string sha = Hash::digest(raw);
    fs::path path = objectPath(sha);
    if (FileUtil::exists(path)) {
        Object old = get(sha);
        if (old.type != type || old.data != data)
            throw runtime_error("Object identity collision");
        return sha;
    }
    fs::create_directory(path.parent_path());
    uLongf size = compressBound(raw.size());
    string packed(size, '\0');
    if (compress2(reinterpret_cast<Bytef*>(&packed[0]), &size,
                  reinterpret_cast<const Bytef*>(raw.data()), raw.size(), Z_BEST_COMPRESSION) != Z_OK)
        throw runtime_error("Compression failed");
    packed.resize(size);
    if (packed.size() > FileUtil::maxSize)
        throw runtime_error("Compressed object exceeds 64 MiB");
    FileUtil::atomicWrite(path, packed);
    return sha;
}

// Enforce a decompression cap before accepting an untrusted object.
Object ObjectStore::get(const string& sha) const {
    string packed = FileUtil::read(objectPath(sha));
    z_stream stream{};
    stream.next_in = reinterpret_cast<Bytef*>(&packed[0]);
    stream.avail_in = static_cast<uInt>(packed.size());
    if (inflateInit(&stream) != Z_OK)
        throw runtime_error("Cannot initialize decompression");
    string raw;
    char buf[65536];
    int result;
    do {
        stream.next_out = reinterpret_cast<Bytef*>(buf);
        stream.avail_out = sizeof(buf);
        result = inflate(&stream, Z_NO_FLUSH);
        size_t count = sizeof(buf) - stream.avail_out;
        if ((result != Z_OK && result != Z_STREAM_END) || raw.size() + count > FileUtil::maxSize) {
            inflateEnd(&stream);
            throw runtime_error("Corrupt or oversized compressed object: " + sha);
        }
        raw.append(buf, count);
    } while (result != Z_STREAM_END);
    bool trailing = stream.avail_in != 0;
    inflateEnd(&stream);
    if (trailing || Hash::digest(raw) != sha)
        throw runtime_error("Object integrity check failed: " + sha);
    size_t end = raw.find('\0');
    size_t space = raw.find(' ');
    if (end == string::npos || space == string::npos || space >= end)
        throw runtime_error("Malformed object header");
    string type = raw.substr(0, space);
    string data = raw.substr(end + 1);
    if ((type != "blob" && type != "tree" && type != "commit") ||
        raw.substr(space + 1, end - space - 1) != to_string(data.size()))
        throw runtime_error("Invalid object type or size");
    return {type, data};
}
