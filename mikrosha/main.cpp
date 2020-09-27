#include <stdio.h>
#include <limits.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <sys/syscall.h>
#include <stdlib.h>

#include <iostream>
#include <algorithm>
#include <string>
#include <vector>

using namespace std;

inline bool is_space(char c){
    return isspace(c);
}

inline bool is_not_space(char c){
    return !isspace(c);
}

vector<string> get_command() {
    string command_line;
    vector<string> args;
    getline(cin, command_line);
    auto first = command_line.begin();
    while (first != command_line.end()) {
        first = find_if(first, command_line.end(), is_not_space);
        auto last = find_if(first, command_line.end(), is_space);
        if (first != command_line.end()) {
            args.push_back(string(first, last));
            first = last;
        }
    }
    return args;
}

int main () {
    chdir("/home/mihail");
    char dir[PATH_MAX];
    getcwd(dir, PATH_MAX);
    printf("%s>", dir);
    vector<string> command_line = get_command(); //parsing
    for (auto &i : command_line) {
        cout<<i<<endl;
    }

}
