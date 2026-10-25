// Library Header Files
#include <chrono>
#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <algorithm>

// Imported Files
#include "../include/commit.h"
#include "../include/hash.h"

using namespace std;

Commit Commit::create(const string& tree, const string& parent, const string& message) {
    const char* name = getenv("MYGIT_AUTHOR_NAME");
    const char* email = getenv("MYGIT_AUTHOR_EMAIL");
    string user = name ? name : "Student";
    string mail = email ? email : "student@localhost";
    if (user.empty() || mail.empty() || user.find_first_of("\r\n<>") != string::npos ||
        mail.find_first_of("\r\n<> ") != string::npos)
        throw runtime_error("Invalid MYGIT_AUTHOR_NAME or MYGIT_AUTHOR_EMAIL");
    auto seconds = chrono::duration_cast<chrono::seconds>(
        chrono::system_clock::now().time_since_epoch()).count();
    return {tree, parent, user + " <" + mail + ">", to_string(seconds), message};
}

string Commit::encode() const {
    string data = "tree " + tree + "\n";
    if (!parent.empty())
        data += "parent " + parent + "\n";
    data += "author " + committer + " " + timestamp + " +0000\n";
    data += "committer " + committer + " " + timestamp + " +0000\n\n" + message;
    return data;
}

Commit Commit::decode(const string& data) {
    size_t split = data.find("\n\n");
    if (split == string::npos)
        throw runtime_error("Malformed commit");
    Commit result;
    string author;
    istringstream input(data.substr(0, split));
    string line;
    while (getline(input, line)) {
        if (line.compare(0, 5, "tree ") == 0 && result.tree.empty())
            result.tree = line.substr(5);
        else if (line.compare(0, 7, "parent ") == 0 && result.parent.empty())
            result.parent = line.substr(7);
        else if (line.compare(0, 7, "author ") == 0 && author.empty())
            author = line.substr(7);
        else if (line.compare(0, 10, "committer ") == 0 && result.committer.empty()) {
            string tail = line.substr(10);
            size_t zone = tail.rfind(' ');
            size_t time = zone == string::npos ? string::npos : tail.rfind(' ', zone - 1);
            if (time == string::npos || zone == string::npos || tail.substr(zone + 1) != "+0000")
                throw runtime_error("Malformed committer metadata");
            result.committer = tail.substr(0, time);
            result.timestamp = tail.substr(time + 1, zone - time - 1);
        }
        else
            throw runtime_error("Unknown or duplicate commit header");
    }
    if (!Hash::valid(result.tree) || (!result.parent.empty() && !Hash::valid(result.parent)) ||
        author.empty() || result.committer.empty() || result.timestamp.empty() ||
        !all_of(result.timestamp.begin(), result.timestamp.end(), [](char ch) { return ch >= '0' && ch <= '9'; }))
        throw runtime_error("Invalid commit metadata");
    result.message = data.substr(split + 2);
    return result;
}
