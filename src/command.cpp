// Library Header Files
#include <iostream>
#include <stdexcept>

// Imported Files
#include "../include/command.h"
#include "../include/repository.h"

using namespace std;

void Command::help() {
    cout << "Mini Version Control System\n"
         << "  ./mygit init\n"
         << "  ./mygit hash-object [-w] <file>\n"
         << "  ./mygit cat-file <-p|-s|-t> <sha>\n"
         << "  ./mygit write-tree\n"
         << "  ./mygit ls-tree [--name-only] <tree_sha>\n"
         << "  ./mygit add <path> [path ...]\n"
         << "  ./mygit commit [-m <message>]\n"
         << "  ./mygit log\n"
         << "  ./mygit checkout <commit_sha>\n";
}

int Command::run(const vector<string>& args) {
    if (args.empty()) {
        help();
        return 1;
    }
    string cmd = args[0];
    if (cmd == "help" || cmd == "--help") {
        if (args.size() != 1)
            throw runtime_error("help takes no arguments");
        help();
        return 0;
    }
    if (cmd == "init") {
        if (args.size() != 1)
            throw runtime_error("Usage: ./mygit init");
        Repository::init();
        return 0;
    }
    bool hashWrite = args.size() == 3 && args[1] == "-w";
    if (cmd == "hash-object" && !hashWrite && args.size() == 2 && args[1][0] != '-') {
        // Git permits hashing without a repository when not writing.
        cout << ObjectStore::hash("blob", FileUtil::read(args[1])) << '\n';
        return 0;
    }
    if (cmd == "hash-object" && !hashWrite)
        throw runtime_error("Usage: ./mygit hash-object [-w] <file>");
    if (cmd == "cat-file" && (args.size() != 3 ||
        (args[1] != "-p" && args[1] != "-s" && args[1] != "-t")))
        throw runtime_error("Usage: ./mygit cat-file <-p|-s|-t> <sha>");
    if ((cmd == "write-tree" || cmd == "log") && args.size() != 1)
        throw runtime_error(cmd + " takes no arguments");
    bool namesOnly = args.size() == 3 && args[1] == "--name-only";
    if (cmd == "ls-tree" && !(args.size() == 2 || namesOnly))
        throw runtime_error("Usage: ./mygit ls-tree [--name-only] <tree_sha>");
    if (cmd == "add" && args.size() < 2)
        throw runtime_error("Usage: ./mygit add <path> [path ...]");
    if (cmd == "commit" && !(args.size() == 1 || (args.size() == 3 && args[1] == "-m")))
        throw runtime_error("Usage: ./mygit commit [-m <message>]");
    if (cmd == "checkout" && args.size() != 2)
        throw runtime_error("Usage: ./mygit checkout <commit_sha>");
    if (cmd != "hash-object" && cmd != "cat-file" && cmd != "write-tree" && cmd != "ls-tree" &&
        cmd != "add" && cmd != "commit" && cmd != "log" && cmd != "checkout")
        throw runtime_error("Unknown command: " + cmd);

    fs::path root = Repository::discover();
    RepoLock lock(root / ".mygit" / "LOCK");
    Repository repo(root);
    if (cmd == "hash-object")
        cout << repo.hashObject(args[2], true) << '\n';
    else if (cmd == "cat-file")
        repo.catFile(args[1], args[2]);
    else if (cmd == "write-tree")
        cout << repo.writeTree() << '\n';
    else if (cmd == "ls-tree")
        repo.lsTree(args.back(), namesOnly);
    else if (cmd == "add")
        repo.add(vector<string>(args.begin() + 1, args.end()));
    else if (cmd == "commit")
        cout << repo.commit(args.size() == 1 ? "Default commit message" : args[2]) << '\n';
    else if (cmd == "log")
        repo.log();
    else
        repo.checkout(args[1]);
    return 0;
}
