// Library Header Files
#include <iostream>
#include <exception>

// Imported Files
#include "../include/command.h"

using namespace std;

int main(int argc, char* argv[]) {
    try {
        Command command;
        return command.run(vector<string>(argv + 1, argv + argc));
    }
    catch (const exception& error) {
        cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
