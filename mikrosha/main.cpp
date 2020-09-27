#include <stdio.h>
#include <limits.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <stdlib.h>

#include <iostream>
#include <algorithm>
#include <string>
#include <vector>

using namespace std;

#define HOME_PATH getenv("HOME")

inline bool is_space(char c)
{
    return isspace(c);
}

inline bool is_not_space(char c)
{
    return !isspace(c);
}

vector<string> split_command(const string &command_line)
{
    vector<string> args;
    auto first = command_line.begin();
    while (first != command_line.end())
    {
        first = find_if(first, command_line.end(), is_not_space);
        auto last = find_if(first, command_line.end(), is_space);
        if (first != command_line.end())
        {
            args.push_back(string(first, last));
            first = last;
        }
    }
    return args;
}
string get_dir()
{
    char dir[PATH_MAX];
    getwd(dir);
    return string(dir);
}
void print_enter_line()
{
    uid_t euid = geteuid();
    char *dir;
    if (euid == 0)
    {
        cout << get_dir() << "!";
    }
    else
    {
        cout << get_dir() << ">";
    }
}

void exec_cd(const string &arg)
{
    int code = chdir(arg.c_str());
    if (code == -1){
        fprintf(stderr, "No such file or directory\n");
    }
    return;
}

void exec_pwd()
{
    printf("%s\n", get_dir);
}
void exec_bash_command(const vector<string> &args)
{
    pid_t pid = fork();
    if (pid == 0)
    {
        vector<char *> v;
        for (size_t i = 0; i < args.size(); i++)
        {
            v.push_back((char *)args[i].c_str());
        }
        v.push_back(NULL);
        execvp(v[0], &v[0]);
        fprintf(stderr, "execution error\n");
        exit(1);
    }
    else
    {
        wait(nullptr);
    }
}
int exec_command(const vector<string> &args)
{
    if (args.empty())
    {
        perror("is_empty");
        return 0;
    }
    if (args[0] == "cd")
    {
        if (args.size() == 1)
        {
            exec_cd(HOME_PATH);
        }
        else if (args.size() == 2)
        {
            exec_cd(args[1]);
        }
        else
        {
            perror("too many arguments\n");
        }
    }
    else if (args[0] == "pwd")
    {
        exec_pwd();
    }
    else if (args[0] == "time")
    {
        perror("time is undefined\n");
    }
    else if (args[0] == "exit")
    {
        return 1;
    }
    else
    {
        exec_bash_command(args);
        //execvp(command_lines[0].c_str(), command_lines[1]);
    }
    return 0;
}



int main()
{
    chdir(HOME_PATH);
    while(true) {
        print_enter_line();
        string command_line;
        getline(cin, command_line);
        if (cin.eof()) {
            cerr << "eof was occured" << endl;
            break;
        }
        vector<string> args = split_command(command_line); //parsing]
        int exit_code = exec_command(args);
        if (exit_code == 1)
        {   
            break;
        }
    }
}
