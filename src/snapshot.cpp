// Library Header Files
#include <stdexcept>
#include <sstream>
#include <set>
#include <algorithm>

// Imported Files
#include "../include/snapshot.h"
#include "../include/hash.h"

using namespace std;

bool Entry::operator==(const Entry& other) const {
    return mode == other.mode && sha == other.sha;
}

void Index::validate(const FileMap& files) {
    if (files.size() > 100000)
        throw runtime_error("Too many files (limit 100000)");
    for (const auto& item : files) {
        if (!FileUtil::safePath(item.first) || !Hash::valid(item.second.sha) ||
            (item.second.mode != "100644" && item.second.mode != "100755"))
            throw runtime_error("Malformed index entry");
        fs::path parent = fs::path(item.first).parent_path();
        size_t depth = 0;
        while (!parent.empty()) {
            if (files.count(parent.generic_string()) || ++depth > 128)
                throw runtime_error("File/directory conflict or excessive nesting in index");
            parent = parent.parent_path();
        }
    }
}

string Index::encode(const FileMap& files) {
    validate(files);
    string data = "MYGIT_INDEX_1\n";
    for (const auto& item : files)
        data += item.second.mode + " " + item.second.sha + "\t" + item.first + "\n";
    return data;
}

FileMap Index::decode(const string& data) {
    istringstream input(data);
    string line;
    if (!getline(input, line) || line != "MYGIT_INDEX_1")
        throw runtime_error("Invalid index header");
    FileMap files;
    while (getline(input, line)) {
        if (line.size() < 49 || line[6] != ' ' || line[47] != '\t')
            throw runtime_error("Invalid index record");
        string name = line.substr(48);
        if (!files.emplace(name, Entry{line.substr(0, 6), line.substr(7, 40)}).second)
            throw runtime_error("Duplicate index record");
    }
    if (data.empty() || data.back() != '\n')
        throw runtime_error("Truncated index");
    validate(files);
    return files;
}

Snapshot::Snapshot(ObjectStore& value) : objects(value) {}

string Snapshot::binarySha(const string& sha) {
    if (!Hash::valid(sha))
        throw runtime_error("Invalid SHA1");
    string data;
    for (size_t i = 0; i < 40; i += 2)
        data += static_cast<char>(stoul(sha.substr(i, 2), nullptr, 16));
    return data;
}

string Snapshot::textSha(const string& bytes) {
    const char* digits = "0123456789abcdef";
    string sha;
    for (unsigned char ch : bytes) {
        sha += digits[ch >> 4];
        sha += digits[ch & 15];
    }
    return sha;
}

vector<TreeEntry> Snapshot::entries(const string& sha) {
    Object object = objects.get(sha);
    if (object.type != "tree")
        throw runtime_error("Expected a tree object");
    vector<TreeEntry> result;
    set<string> names;
    size_t pos = 0;
    string previous;
    while (pos < object.data.size()) {
        size_t space = object.data.find(' ', pos);
        size_t zero = object.data.find('\0', pos);
        if (space == string::npos || zero == string::npos || space >= zero ||
            object.data.size() - zero - 1 < 20)
            throw runtime_error("Malformed tree object");
        string mode = object.data.substr(pos, space - pos);
        string name = object.data.substr(space + 1, zero - space - 1);
        if (!FileUtil::safeName(name) || !names.insert(name).second ||
            (mode != "40000" && mode != "100644" && mode != "100755"))
            throw runtime_error("Unsafe name, duplicate entry or unsupported tree mode");
        string key = name + (mode == "40000" ? "/" : "");
        if (!previous.empty() && key <= previous)
            throw runtime_error("Tree entries are not sorted");
        previous = key;
        result.push_back({name, {mode, textSha(object.data.substr(zero + 1, 20))}});
        pos = zero + 21;
    }
    return result;
}

string Snapshot::buildAt(const FileMap& files, const string& prefix, size_t depth) {
    if (depth > 128)
        throw runtime_error("Directory nesting exceeds 128");
    map<string, Entry> values;
    set<string> dirs;
    for (const auto& item : files) {
        if (item.first.compare(0, prefix.size(), prefix) != 0)
            continue;
        string tail = item.first.substr(prefix.size());
        size_t slash = tail.find('/');
        if (slash == string::npos) {
            if (objects.get(item.second.sha).type != "blob")
                throw runtime_error("Index references a non-blob");
            values[tail] = item.second;
        }
        else {
            dirs.insert(tail.substr(0, slash));
        }
    }
    for (const string& name : dirs)
        values[name] = {"40000", buildAt(files, prefix + name + "/", depth + 1)};
    vector<TreeEntry> sorted;
    for (const auto& item : values)
        sorted.push_back({item.first, item.second});
    sort(sorted.begin(), sorted.end(), [](const TreeEntry& a, const TreeEntry& b) {
        return a.name + (a.value.mode == "40000" ? "/" : "") <
               b.name + (b.value.mode == "40000" ? "/" : "");
    });
    string data;
    for (const auto& item : sorted)
        data += item.value.mode + " " + item.name + '\0' + binarySha(item.value.sha);
    return objects.put("tree", data);
}

string Snapshot::build(const FileMap& files) {
    Index::validate(files);
    return buildAt(files, "", 0);
}

void Snapshot::readAt(const string& sha, const string& prefix, FileMap& files, size_t depth) {
    if (depth > 128 || ++visitedTrees > 100000)
        throw runtime_error("Tree nesting or traversal count exceeds limit");
    for (const auto& item : entries(sha)) {
        string name = prefix + item.name;
        if (item.value.mode == "40000") {
            readAt(item.value.sha, name + "/", files, depth + 1);
        }
        else {
            if (objects.get(item.value.sha).type != "blob")
                throw runtime_error("Tree references a non-blob");
            files.emplace(name, item.value);
            if (files.size() > 100000)
                throw runtime_error("Too many files in tree");
        }
    }
}

FileMap Snapshot::read(const string& sha) {
    visitedTrees = 0;
    FileMap files;
    readAt(sha, "", files, 0);
    Index::validate(files);
    return files;
}
