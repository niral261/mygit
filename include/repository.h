#ifndef REPOSITORY_H
#define REPOSITORY_H

// Imported Files
#include "snapshot.h"
#include "commit.h"

class Repository {
private:
    fs::path root;
    fs::path meta;
    ObjectStore objects;
    Snapshot snapshots;

    FileMap index() const;
    string head() const;
    Commit readCommit(const string& sha) const;
    FileMap headFiles();
    fs::path resolve(const string& path) const;
    void checkParents(const fs::path& path) const;
    void scan(const fs::path& path, FileMap& files, size_t depth);
    static bool under(const string& name, const string& prefix);
    void removeEmptyParents(const fs::path& path) const;

public:
    static fs::path discover();
    static void init();
    explicit Repository(const fs::path& root);
    string hashObject(const string& path, bool write);
    void catFile(const string& flag, const string& sha);
    string writeTree();
    void lsTree(const string& sha, bool namesOnly);
    void add(const vector<string>& paths);
    string commit(const string& message);
    void log();
    void checkout(const string& sha);
};

#endif
