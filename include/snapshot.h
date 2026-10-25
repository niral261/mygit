#ifndef SNAPSHOT_H
#define SNAPSHOT_H

// Library Header Files
#include <map>
#include <vector>

// Imported Files
#include "objectStore.h"

struct Entry {
    string mode;
    string sha;
    bool operator==(const Entry& other) const;
};

using FileMap = map<string, Entry>;

struct TreeEntry {
    string name;
    Entry value;
};

// Index serialization and validation; index contains the complete staged snapshot.
class Index {
public:
    static string encode(const FileMap& files);
    static FileMap decode(const string& data);
    static void validate(const FileMap& files);
};

// Encode/decode Git trees and flatten recursive trees into repository paths.
class Snapshot {
private:
    ObjectStore& objects;
    size_t visitedTrees = 0;
    string buildAt(const FileMap& files, const string& prefix, size_t depth);
    void readAt(const string& sha, const string& prefix, FileMap& files, size_t depth);
public:
    explicit Snapshot(ObjectStore& objects);
    static string binarySha(const string& sha);
    static string textSha(const string& bytes);
    vector<TreeEntry> entries(const string& sha);
    string build(const FileMap& files);
    FileMap read(const string& sha);
};

#endif
