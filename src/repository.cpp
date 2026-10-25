// Library Header Files
#include <iostream>
#include <stdexcept>
#include <set>
#include <algorithm>

// Imported Files
#include "../include/repository.h"
#include "../include/hash.h"

using namespace std;

fs::path Repository::discover() {
    fs::path path = fs::current_path();
    for (;;) {
        if (FileUtil::exists(path / ".mygit")) {
            FileUtil::directory(path / ".mygit");
            return path;
        }
        if (path == path.root_path())
            throw runtime_error("Not inside a mygit repository; run ./mygit init first");
        path = path.parent_path();
    }
}

void Repository::init() {
    fs::path meta = fs::current_path() / ".mygit";
    if (FileUtil::exists(meta)) {
        Repository repo(fs::current_path());
        RepoLock lock(meta / "LOCK");
        repo.index();
        repo.head();
        cout << "Repository already initialized\n";
        return;
    }
    // Prepare all files before publishing the repository directory.
    fs::path temp = fs::current_path() / ".mygit.init";
    if (!fs::create_directory(temp))
        throw runtime_error("Initialization temporary directory already exists");
    try {
        fs::create_directory(temp / "objects");
        FileUtil::write(temp / "format", "MYGIT_1\n");
        FileUtil::write(temp / "HEAD", "\n");
        FileUtil::write(temp / "index", Index::encode({}));
        fs::rename(temp, meta);
    }
    catch (...) {
        fs::remove_all(temp);
        throw;
    }
    cout << "Initialized empty repository in " << meta.string() << '\n';
}

Repository::Repository(const fs::path& value)
    : root(value), meta(root / ".mygit"), objects(meta / "objects"), snapshots(objects) {
    FileUtil::directory(meta);
    if (FileUtil::read(meta / "format") != "MYGIT_1\n")
        throw runtime_error("Unsupported repository format");
    if (FileUtil::exists(meta / "checkoutTxn"))
        throw runtime_error("Interrupted checkout found; see README recovery instructions before continuing");
}

FileMap Repository::index() const {
    return Index::decode(FileUtil::read(meta / "index"));
}

string Repository::head() const {
    string data = FileUtil::read(meta / "HEAD");
    if (data == "\n")
        return "";
    if (data.size() != 41 || data.back() != '\n' || !Hash::valid(data.substr(0, 40)))
        throw runtime_error("Malformed HEAD");
    return data.substr(0, 40);
}

Commit Repository::readCommit(const string& sha) const {
    Object object = objects.get(sha);
    if (object.type != "commit")
        throw runtime_error("Expected a commit object");
    return Commit::decode(object.data);
}

FileMap Repository::headFiles() {
    string sha = head();
    return sha.empty() ? FileMap{} : snapshots.read(readCommit(sha).tree);
}

bool Repository::under(const string& name, const string& prefix) {
    return prefix.empty() || name == prefix || name.compare(0, prefix.size() + 1, prefix + "/") == 0;
}

void Repository::checkParents(const fs::path& path) const {
    fs::path current = root;
    for (const auto& part : path.lexically_relative(root).parent_path()) {
        current /= part;
        if (FileUtil::exists(current))
            FileUtil::directory(current);
    }
}

fs::path Repository::resolve(const string& path) const {
    if (path.empty())
        throw runtime_error("Empty path");
    fs::path absolute = fs::absolute(path).lexically_normal();
    // lexical normalization can retain a trailing slash; use one root representation.
    while (absolute != absolute.root_path() && absolute.filename().empty())
        absolute = absolute.parent_path();
    fs::path rel = absolute.lexically_relative(root);
    if (rel != "." && !FileUtil::safePath(rel.generic_string()))
        throw runtime_error("Path is outside repository, reserved or unsafe: " + path);
    checkParents(absolute);
    return absolute;
}

void Repository::scan(const fs::path& path, FileMap& files, size_t depth) {
    if (depth > 128)
        throw runtime_error("Directory nesting exceeds 128");
    string rel = path.lexically_relative(root).generic_string();
    if (rel == ".mygit" || rel == ".git" || rel == "mygit")
        return;
    auto status = fs::symlink_status(path);
    if (fs::is_directory(status)) {
        vector<fs::path> children;
        for (const auto& item : fs::directory_iterator(path)) {
            string name = item.path().filename().string();
            if (name == ".mygit" || name == ".git")
                continue;
            if (!FileUtil::safeName(name))
                throw runtime_error("Unsupported filename: " + name);
            children.push_back(item.path());
        }
        sort(children.begin(), children.end());
        for (const auto& child : children)
            scan(child, files, depth + 1);
    }
    else if (fs::is_regular_file(status)) {
        files[rel] = {FileUtil::executable(path) ? "100755" : "100644",
                      objects.put("blob", FileUtil::read(path))};
        if (files.size() > 100000)
            throw runtime_error("Too many files");
    }
    else {
        throw runtime_error("Only real directories and regular files are supported: " + rel);
    }
}

string Repository::hashObject(const string& path, bool write) {
    fs::path target = fs::absolute(path).lexically_normal();
    // Hashing is a read-only operation; an external regular file is allowed.
    string data = FileUtil::read(target);
    return write ? objects.put("blob", data) : ObjectStore::hash("blob", data);
}

void Repository::catFile(const string& flag, const string& sha) {
    Object object = objects.get(sha);
    if (flag == "-t")
        cout << object.type << '\n';
    else if (flag == "-s")
        cout << object.data.size() << '\n';
    else if (object.type == "tree")
        lsTree(sha, false);
    else
        cout.write(object.data.data(), object.data.size());
}

string Repository::writeTree() {
    FileMap files;
    scan(root, files, 0);
    // Assignment asks for working-directory state, rather than Git's index tree.
    return snapshots.build(files);
}

void Repository::lsTree(const string& sha, bool namesOnly) {
    for (const auto& item : snapshots.entries(sha)) {
        if (!namesOnly) {
            cout << (item.value.mode == "40000" ? "040000" : item.value.mode) << ' '
                 << (item.value.mode == "40000" ? "tree" : "blob") << ' '
                 << item.value.sha << '\t';
        }
        cout << item.name << '\n';
    }
}

void Repository::add(const vector<string>& paths) {
    FileMap staged = index();
    for (const string& value : paths) {
        fs::path path = resolve(value);
        string prefix = path == root ? "" : path.lexically_relative(root).generic_string();
        if (prefix == "mygit")
            throw runtime_error("The root mygit executable is excluded");
        bool found = false;
        for (auto it = staged.begin(); it != staged.end();) {
            if (under(it->first, prefix)) {
                found = true;
                it = staged.erase(it);
            }
            else
                ++it;
        }
        if (FileUtil::exists(path)) {
            // Replace a previously staged ancestor file when it becomes a directory.
            fs::path parent = fs::path(prefix).parent_path();
            while (!parent.empty()) {
                staged.erase(parent.generic_string());
                parent = parent.parent_path();
            }
            scan(path, staged, 0);
        }
        else if (!found) {
            throw runtime_error("Path does not exist and is not staged: " + value);
        }
    }
    FileUtil::atomicWrite(meta / "index", Index::encode(staged));
    cout << "Staging area updated\n";
}

string Repository::commit(const string& message) {
    FileMap staged = index();
    FileMap previous = headFiles();
    string parent = head();
    if (staged == previous)
        throw runtime_error("Nothing to commit; stage a change with add first");
    string tree = snapshots.build(staged);
    Commit value = Commit::create(tree, parent, message);
    string sha = objects.put("commit", value.encode());
    FileUtil::atomicWrite(meta / "HEAD", sha + "\n");
    return sha;
}

void Repository::log() {
    string sha = head();
    if (sha.empty()) {
        cout << "No commits yet\n";
        return;
    }
    set<string> seen;
    while (!sha.empty()) {
        if (!seen.insert(sha).second || seen.size() > 100000)
            throw runtime_error("Cyclic or excessive commit history");
        Commit value = readCommit(sha);
        cout << "commit " << sha << '\n';
        if (!value.parent.empty())
            cout << "Parent: " << value.parent << '\n';
        cout << "Committer: " << value.committer << '\n'
             << "Timestamp: " << value.timestamp << " +0000 (Unix seconds, UTC)\n\n"
             << value.message << "\n\n";
        sha = value.parent;
    }
}

void Repository::removeEmptyParents(const fs::path& path) const {
    fs::path dir = path.parent_path();
    while (dir != root && dir != meta && fs::is_directory(fs::symlink_status(dir)) && fs::is_empty(dir)) {
        fs::remove(dir);
        dir = dir.parent_path();
    }
}

void Repository::checkout(const string& sha) {
    FileMap target = snapshots.read(readCommit(sha).tree);
    FileMap current = headFiles();
    if (index() != current)
        throw runtime_error("Staged changes would be lost; commit them before checkout");

    // Validate every old tracked file and every destination before changing anything.
    for (const auto& item : current) {
        fs::path path = root / item.first;
        checkParents(path);
        if (!FileUtil::exists(path) || !fs::is_regular_file(fs::symlink_status(path)) ||
            ObjectStore::hash("blob", FileUtil::read(path)) != item.second.sha ||
            (FileUtil::executable(path) ? "100755" : "100644") != item.second.mode)
            throw runtime_error("Local change would be lost: " + item.first);
    }
    for (const auto& item : target) {
        fs::path path = root / item.first;
        if (item.first == "mygit")
            throw runtime_error("Target contains the excluded executable path");
        fs::path prefix;
        for (const auto& part : fs::path(item.first)) {
            prefix /= part;
            fs::path actual = root / prefix;
            if (!FileUtil::exists(actual))
                continue;
            auto status = fs::symlink_status(actual);
            if (fs::is_symlink(status))
                throw runtime_error("Symlink blocks checkout: " + prefix.string());
            if (!fs::is_directory(status) && !current.count(prefix.generic_string()))
                throw runtime_error("Untracked path blocks checkout: " + prefix.string());
            if (!fs::is_directory(status))
                break;
        }
        if (FileUtil::exists(path) && fs::is_directory(fs::symlink_status(path))) {
            for (const auto& child : fs::recursive_directory_iterator(path)) {
                if (!fs::is_directory(child.symlink_status()) &&
                    !current.count(child.path().lexically_relative(root).generic_string()))
                    throw runtime_error("Untracked directory contents block checkout: " + item.first);
            }
        }
    }

    fs::path txn = meta / "checkoutTxn";
    if (!fs::create_directory(txn))
        throw runtime_error("Checkout transaction already exists");
    vector<string> moved;
    vector<string> installed;
    vector<fs::path> removedDirs;
    string oldIndex = FileUtil::read(meta / "index");
    string oldHead = FileUtil::read(meta / "HEAD");
    bool indexUpdated = false;
    bool headUpdated = false;
    try {
        fs::create_directory(txn / "old");
        fs::create_directory(txn / "new");
        FileUtil::write(txn / "index", oldIndex);
        FileUtil::write(txn / "HEAD", oldHead);
        FileUtil::write(txn / "targetIndex", Index::encode(target));
        // Materialize all target blobs before moving any working files.
        for (const auto& item : target) {
            fs::path path = txn / "new" / item.first;
            fs::create_directories(path.parent_path());
            FileUtil::write(path, objects.get(item.second.sha).data);
            FileUtil::setMode(path, item.second.mode);
        }
        FileUtil::write(txn / "READY", "Ready\n");
        for (const auto& item : current) {
            fs::path backup = txn / "old" / item.first;
            fs::create_directories(backup.parent_path());
            fs::rename(root / item.first, backup);
            moved.push_back(item.first);
        }
        // Remove only directories which must become files, once proven free of untracked files.
        for (const auto& item : target) {
            fs::path path = root / item.first;
            if (FileUtil::exists(path) && fs::is_directory(fs::symlink_status(path))) {
                for (const auto& child : fs::recursive_directory_iterator(path))
                    removedDirs.push_back(child.path());
                removedDirs.push_back(path);
                fs::remove_all(path);
            }
            fs::create_directories(path.parent_path());
            fs::rename(txn / "new" / item.first, path);
            installed.push_back(item.first);
        }
        FileUtil::atomicWrite(meta / "index", Index::encode(target));
        indexUpdated = true;
        FileUtil::atomicWrite(meta / "HEAD", sha + "\n");
        headUpdated = true;
    }
    catch (...) {
        // Roll back handled I/O failures. Keep backups if rollback itself fails.
        try {
            for (const string& name : installed) {
                fs::remove(root / name);
                removeEmptyParents(root / name);
            }
            for (const auto& dir : removedDirs)
                fs::create_directories(dir);
            for (const string& name : moved) {
                fs::path path = root / name;
                fs::create_directories(path.parent_path());
                fs::rename(txn / "old" / name, path);
            }
            if (indexUpdated)
                FileUtil::atomicWrite(meta / "index", oldIndex);
            if (headUpdated)
                FileUtil::atomicWrite(meta / "HEAD", oldHead);
            fs::remove_all(txn);
        }
        catch (...) {
            throw runtime_error("Checkout and rollback failed; backups retained in .mygit/checkoutTxn");
        }
        throw;
    }
    for (const string& name : moved)
        removeEmptyParents(root / name);
    fs::remove_all(txn);
    cout << "Checked out " << sha << '\n';
}
