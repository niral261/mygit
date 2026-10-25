#ifndef COMMAND_H
#define COMMAND_H

// Library Header Files
#include <string>
#include <vector>

using namespace std;

class Command {
public:
    static void help();
    int run(const vector<string>& args);
};

#endif
