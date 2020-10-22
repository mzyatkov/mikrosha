#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/rtc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/prctl.h>

#include <algorithm>
#include <cassert>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
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

class Command
{
public:
    vector<string> all_args;
    vector<string> command_args;      // "command < file1 > file2", command empty
                                      //  in case no redirection
    vector<pair<string, bool>> files; // file args: 0 - if input, 1 - if output
    int fd_in = -1, fd_out = -1;
    Command(const string &input)
    {
        split_line(input);
        split_redir();
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
    bool is_empty() { return command_args.empty(); }
    bool is_redir() { return isredir; }
    bool is_cd() { return is_empty() ? false : command_args[0] == "cd"; }
    bool is_time() { return is_empty() ? false : command_args[0] == "time"; }
    bool is_pwd() { return is_empty() ? false : command_args[0] == "pwd"; }
    void delete_time()
    {
        if (is_time())
        {
            command_args.erase(command_args.begin());
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
            if (command_args.size() == 1)
            {
                exec_cd(HOME_PATH);
            }
            else if (command_args.size() == 2)
            {
                exec_cd(command_args[1]);
            }
            else
            {
                for (auto &i : command_args)
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
            exec_bash_command(command_args);
        }
        return 0;
    }
    //do redirection stdio to files, open file descriptors
    void prepare_command_if_redir()
    {
        if (!is_redir())
        {
            return;
        }
        bool input_exists = false, output_exists = false;
        string input_name, output_name;
        for (int i = files.size() - 1; i >= 0; i--)
        {
            if (files[i].second == 0 && !input_exists)
            {
                input_exists = true;
                input_name = files[i].first;
            }
            else if (files[i].second == 1 && !output_exists)
            {
                output_exists = true;
                output_name = files[i].first;
            }
        }
        fd_out = -1, fd_in = -1;
        if (output_exists)
        {
            fd_out = open(output_name.c_str(), O_WRONLY | O_TRUNC | O_CREAT, 644);
            if (fd_out == -1)
            {
                perror("open output file failed");
                return;
            }
            dup2(fd_out, STDOUT_FILENO);
        }
        if (input_exists)
        {
            fd_in = open(input_name.c_str(), O_RDONLY);
            if (fd_in == -1)
            {
                perror("open input file failed");
                return;
            }
            dup2(fd_in, STDIN_FILENO);
        }
        return;
    }

private:
    bool isredir = true;
    // split args by spaces
    void split_line(const string &command_line)
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
        prctl(PR_SET_PDEATHSIG, SIGINT);//exit process when parent dies
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
    void expand_reg_expr()
    {
        vector<vector<string>> graph_reg_expr;
        for (auto &arg : all_args)
        {
            if (arg.find('*') != arg.npos || arg.find('?') != arg.npos)
            {
                arg = expand_reg_expr_arg(arg);
            }
        }
        return;
    }
    string expand_reg_expr_arg(string expr)
    {
        string cur_dir = get_dir();
        if (expr[0] == '/')
        {
        }
        return "";
    }
    // split args into {command}, {input_files} and {output_files}
    void split_redir()
    {
        auto less_it = find(all_args.begin(), all_args.end(), "<");
        auto more_it = find(all_args.begin(), all_args.end(), ">");
        if (less_it < more_it)
        {
            command_args.assign(all_args.begin(), less_it);
        }
        else if (less_it > more_it)
        {
            command_args.assign(all_args.begin(), more_it);
        }
        else
        {
            command_args = all_args;
            isredir = false;
            return;
        }
        while (less_it != all_args.end() || more_it != all_args.end())
        {
            if (less_it < all_args.end() - 1)
            {
                files.push_back(make_pair(*(less_it + 1), 0));
            }
            if (more_it < all_args.end() - 1)
            {
                files.push_back(make_pair(*(more_it + 1), 1));
            }
            if (less_it != all_args.end())
            {
                less_it++;
            }
            if (more_it != all_args.end())
            {
                more_it++;
            }
            less_it = find(less_it, all_args.end(), "<");
            more_it = find(more_it, all_args.end(), ">");
        }
        return;
    }
};

vector<string> split_line_by_char(const string &command_line, const char delim)
{
    vector<string> splitted_line;
    string token;
    int cur_ptr = 0, prev_ptr = 0;
    while ((cur_ptr = command_line.find(delim, prev_ptr)) != string::npos)
    {
        token = command_line.substr(prev_ptr, cur_ptr - prev_ptr);
        splitted_line.push_back(token);
        prev_ptr = cur_ptr + 1;
    }
    splitted_line.push_back(command_line.substr(prev_ptr, command_line.length() - prev_ptr));
    return splitted_line;
}
void print_enter_line()
{
    uid_t euid = geteuid();
    char *dir;
    // cout << euid << " :asdfasfd" << endl;
    if (euid == 0)
    {
        cout << get_dir() << "!";
    }
    else
    {
        cout << get_dir() << ">";
    }
}

int  exec_conveyer(vector<Command> &commands)
{
    if (commands.empty() || commands[0].is_empty())
    {
        return 0;
    }
    if (commands[0].is_time())
    {
        commands[0].delete_time();
        struct rusage start, stop;
        struct timeval start_real_time, stop_real_time;
        getrusage(RUSAGE_CHILDREN, &start);
        gettimeofday(&start_real_time, NULL);
        ////////
        exec_conveyer(commands);
        ////////
        gettimeofday(&stop_real_time, NULL);
        getrusage(RUSAGE_CHILDREN, &stop);
        auto real_time_sec = stop_real_time.tv_sec - start_real_time.tv_sec;
        auto real_time_usec = stop_real_time.tv_usec - start_real_time.tv_usec;
        cerr << "real: " << real_time_sec << "." << setfill('0') << setw(3)
             << real_time_usec << endl
             << "system: " << stop.ru_stime.tv_sec - start.ru_stime.tv_sec << "." << setfill('0')
             << setw(3) << stop.ru_stime.tv_usec - start.ru_stime.tv_usec<< endl
             << "user: " << stop.ru_utime.tv_sec - start.ru_utime.tv_sec << "." << setfill('0') << setw(3)
             << stop.ru_utime.tv_usec - start.ru_utime.tv_usec<< endl;
        return 0;
    }
    int stored_stdin = dup(STDIN_FILENO); // remember original std file descriptors
    int stored_stdout = dup(STDOUT_FILENO);
    if (commands.size() == 1)
    {
        if (commands[0].is_cd())
        {
            commands[0].exec_command();
            return 0;
        }
        vector<int> fds; //file descriptors in and out
        commands[0].prepare_command_if_redir();
        //do executing
        if (fork() == 0)
        {
            commands[0].exec_command();
        }
        else
        {
            wait(nullptr);
            commands[0].fd_in != -1 ? close(commands[0].fd_in) : 0;
            commands[0].fd_out != -1 ? close(commands[0].fd_out) : 0;
        }
    }
    else
    {
        vector<int[2]> pipes(commands.size() - 1);
        int cur_fd = 0, prev_fd = -1;
        for (auto &fd : pipes)
        {
            if (pipe2(fd, O_CLOEXEC) != 0)
            {
                perror("pipe\n");
            }
        }
        for (int i = 0; i < commands.size(); i++, cur_fd++, prev_fd++)
        {
            commands[i].prepare_command_if_redir();
            pid_t pid = fork();
            if (pid == 0)
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
                if (i == commands.size() - 1)
                {
                    for (auto &i : pipes)
                    {
                        close(i[0]);
                        close(i[1]);
                    }
                    for (int i = 0; i < commands.size(); i++)
                    {
                        wait(NULL);
                    }
                }
            }
            commands[0].fd_in != -1 ? close(commands[0].fd_in) : 0;//close if opened
            commands[0].fd_out != -1 ? close(commands[0].fd_out) : 0;
        }
    }
    dup2(stored_stdin, STDIN_FILENO);
    dup2(stored_stdout, STDOUT_FILENO);
    close(stored_stdin);
    close(stored_stdout);
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
            vector<string> splitted_conveyer = split_line_by_char(input, '|');
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
