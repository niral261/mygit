#ifndef COMMIT_H
#define COMMIT_H

// Library Header Files
#include <string>

using namespace std;

class Commit {
public:
    string tree;
    string parent;
    string committer;
    string timestamp;
    string message;

    static Commit create(const string& tree, const string& parent, const string& message);
    static Commit decode(const string& data);
    string encode() const;
};

#endif
