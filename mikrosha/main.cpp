#include <stdio.h>
#include <limits.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <linux/rtc.h>

#include <iostream>
#include <algorithm>
#include <string>
#include <vector>
#include <fstream>
#include <cassert>

using namespace std;

#define HOME_PATH getenv("HOME")

string get_dir()
{
    char *dir;
    dir = get_current_dir_name();
    string str(dir);
    free(dir);
    return str;
}

vector<string> split_conveyer(const string &command_line)
{
    vector<string> splitted_conveyer;
    auto first = command_line.begin();
    while (first != command_line.end())
    {
        first = find_if_not(first, command_line.end(), [](auto c) { return (c == '|') ? true : false; });
        auto last = find_if(first, command_line.end(), [](auto c) { return (c == '|') ? true : false; });
        if (first != command_line.end())
        {
            splitted_conveyer.push_back(string(first, last));
            first = last;
        }
    }
    return splitted_conveyer;
}

class Command
{
public:
    string command_line;
    vector<string> all_args;
    int fd[2][2]; //file descriptor for input/output in conveyer
    Command(const string &input)
    {
        this->command_line = input;
        this->split_line();
    }
    ~Command() {}
    void print()
    {
        for (auto &i : all_args)
        {
            cout << cout.width(2) << i;
        }
        cout << endl;
    }
    bool is_empty() {
        return all_args.empty();
    }
    bool is_cd()
    {
        return is_empty() ? false : all_args[0] == "cd";
    }
    bool is_time()
    {
        return is_empty() ? false : all_args[0] == "time";
    }
    bool is_pwd()
    {
        return is_empty() ? false : all_args[0] == "pwd";
    }
    void delete_time() {
        if(is_time()) {
            all_args.erase(all_args.begin());
        }
        return;
    }
    int exec_command()
    {
        if (is_empty())
        {
            perror("is_empty");
            return 0;
        }
        if (is_cd())
        {
            if (all_args.size() == 1)
            {
                exec_cd(HOME_PATH);
            }
            else if (all_args.size() == 2)
            {
                exec_cd(all_args[1]);
            }
            else
            {
                for (auto &i : all_args)
                {
                    cout << i << endl;
                }
                perror("too many arguments\n");
            }
        }
        else if (is_pwd())
        {
            exec_pwd();
        }
        else
        {
            exec_bash_command(all_args);
        }
        return 0;
    }

private:
    void split_line()
    {
        auto first = command_line.begin();
        while (first != command_line.end())
        {
            first = find_if_not(first, command_line.end(), ::isspace);
            auto last = find_if(first, command_line.end(), ::isspace);
            if (first != command_line.end())
            {
                all_args.push_back(string(first, last));
                first = last;
            }
        }
    }
    void exec_pwd()
    {
        cout << get_dir() << endl;
        exit(0);
    }
    void exec_bash_command(const vector<string> &args)
    {
        vector<char *> v;
        for (size_t i = 0; i < args.size(); i++)
        {
            v.push_back((char *)args[i].c_str());
        }
        v.push_back(NULL);
        // int fd=open(inputfile,O_WRONLY|O_CREAT);
        // dup2(fd,1);
        execvp(v[0], &v[0]);
        cerr << "Execution error" << endl;
        exit(1);
    }
    void exec_cd(const string &arg)
    {
        int code = chdir(arg.c_str());
        if (code == -1)
        {
            cerr << "No such file or directory" << endl;
        }
    }
};
void print_enter_line()
{
    uid_t euid = geteuid();
    char *dir;
    cout << euid << " :asdfasfd" << endl;
    if (euid == 0)
    {
        cout << get_dir() << "!";
    }
    else
    {
        cout << get_dir() << ">";
    }
}
int exec_conveyer(vector<Command> commands)
{
    int fd[2][2];
    if (commands.empty() || commands[0].is_empty())
    {
        return 0;
    }
    if (commands.size() == 1)
    {
        if (commands[0].is_cd())
        {
            commands[0].exec_command();
            return 0;
        }
        if (commands[0].is_time())
        {
            struct rusage usage;
            auto start = time(NULL);
            getrusage(RUSAGE_CHILDREN, &usage);
            auto start_user_time = usage.ru_utime.tv_sec;
            auto start_sys_time = usage.ru_stime.tv_sec;
            ////////
            commands[0].delete_time();
            exec_conveyer(commands);
            ////////
            auto stop = time(NULL);
            getrusage(RUSAGE_CHILDREN, &usage);
            double user_time = difftime(usage.ru_utime.tv_sec ,user_time);
            double sys_time = difftime(usage.ru_stime.tv_sec, sys_time);
            double real_time = difftime(stop, start);
            cerr << "real: "<< real_time << endl 
            << "system: " << sys_time << endl
            << "user: " << user_time << endl;
            return 0;
        }
        if (fork() == 0)
        {
            commands[0].exec_command();
        }
        else
        {
            wait(nullptr);
        }
    }
    else
    {
        vector<int[2]> pipes(commands.size());
        int cur_fd = 0, prev_fd = -1;
        for (auto &fd : pipes)
        {
            if (pipe(fd) != 0)
            {
                perror("pipe\n");
            }
        }
        for (int i = 0; i < commands.size(); i++, cur_fd++, prev_fd++)
        {
            if (fork() == 0)
            {
                if (i == 0)
                {
                    dup2(pipes[0][1], STDOUT_FILENO);
                    close(pipes[0][0]);
                    commands[i].exec_command();
                }
                if ((i > 0) && (i < commands.size() - 1))
                {
                    dup2(pipes[prev_fd][0], STDIN_FILENO);
                    close(pipes[prev_fd][1]);
                    dup2(pipes[cur_fd][1], STDOUT_FILENO);
                    close(pipes[cur_fd][0]);
                    commands[i].exec_command();
                }
                if (i == commands.size() - 1)
                {
                    dup2(pipes[prev_fd][0], STDIN_FILENO);
                    close(pipes[prev_fd][1]);
                    commands[i].exec_command();
                }
            }
            else
            {
                wait(nullptr);
            }
        }
        for (auto &pipe : pipes)
        {
            close(pipe[0]);
            close(pipe[1]);
        }
        // if (fork() == 0)
        // {
        //     if (commands.size() == 2)
        //     {
        //         dup2(fd[1], STDOUT_FILENO);
        //         close(fd[0]);
        //         commands[0].exec_command();
        //     }

        // for (int i = 1; i < commands.size(); i++)
        // {
        //     pipe(fd[i % 2]);
        //     pid_t pid = fork();
        //     if (pid != 0)
        //     { // parent?
        //         close(fd[i % 2][0]);
        //         dup2(fd[i % 2][1], STDOUT_FILENO);
        //         commands[i].exec_command();
        //     }
        //     else
        //     {
        //         close(fd[i % 2][1]);
        //         dup2(fd[i % 2][0], STDIN_FILENO);
        //         commands[i + 1].exec_command();
        //     }
        //     wait(nullptr);
        //     close(fd[0][0]);
        //     close(fd[0][1]);
        // }

        // int cur_fd = 0;
        // for (int i = 0; i < commands.size(); i++)
        // {

        //     pipe(fd[cur_fd]);
        //     if (fork() == 0)
        //     {
        //         if (fork() == 0)
        //         {
        //             dup2(fd[cur_fd][1], STDOUT_FILENO);
        //             close(fd[cur_fd][0]);

        //             commands[i].exec_command();
        //             cerr << "executed0..." << endl;
        //             exit(0);
        //         }
        //         else
        //         {
        //             wait(nullptr);
        //             cout << "executed1..." << endl;

        //             dup2(fd[cur_fd][0], STDIN_FILENO);
        //             close(fd[cur_fd][1]);

        //             cout << "waiting..." << endl;
        //             wait(nullptr);
        //             cout << "executing..." << endl;
        //             commands[i + 1].exec_command();
        //             cout << "executed..." << endl;
        //             exit(0);
        //         }
        //     }
        //     else
        //     {
        //         wait(nullptr);
        //     }
        // }
    }
    return 0;
}

int main()
{
    chdir(HOME_PATH);
    while (true)
    {
        print_enter_line();
        try
        {
            string input;
            if (!getline(cin, input))
            {
                throw "EOF";
            }
            vector<string> splitted_conveyer = split_conveyer(input);
            vector<Command> commands;
            for (const auto &i : splitted_conveyer)
            {
                commands.push_back(i);
            }
            exec_conveyer(commands);
        }
        catch (char const *a)
        {
            cerr << a << " was occured" << endl;
            break;
        }
    }
}
