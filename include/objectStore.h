#ifndef OBJECT_STORE_H
#define OBJECT_STORE_H

// Imported Files
#include "fileUtil.h"

struct Object {
    string type;
    string data;
};

// Git-compatible SHA1 envelope and zlib-compressed loose objects.
class ObjectStore {
private:
    fs::path root;
    fs::path objectPath(const string& sha) const;
public:
    explicit ObjectStore(const fs::path& root);
    static string hash(const string& type, const string& data);
    string put(const string& type, const string& data);
    Object get(const string& sha) const;
};

#endif
