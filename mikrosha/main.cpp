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
#include <fcntl.h>
#include <sys/stat.h>

#include <iostream>
#include <algorithm>
#include <string>
#include <cstring>
#include <vector>
#include <fstream>
#include <cassert>
#include <iomanip>
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
    string command_line;
    vector<string> all_args;
    vector<string> command_args;      // "command < file1 > file2", command empty in case no redirection
    vector<pair<string, bool>> files; //file args: 0 - if input, 1 - if output
    Command(const string &input)
    {
        command_line = input;
        split_line();
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
    bool is_empty()
    {
        return all_args.empty();
    }
    bool is_redir()
    {
        return isredir;
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
    void delete_time()
    {
        if (is_time())
        {
            this->all_args.erase(all_args.begin());
        }
        return;
    }
    int exec_command()
    {
        if (isredir)
        {
            bool input_exists=false, output_exists=false;
            char input_name[256], output_name[256];
            for (int i = files.size()-1; i>=0; i--) {
                if (files[i].second == 0 && !input_exists) {
                    input_exists = true;
                    strncpy(input_name, files[i].first.c_str(), 255);
                }else if(files[i].second == 1 && !output_exists) {
                    output_exists = true;
                    strncpy(output_name, files[i].first.c_str(), 255);
                }
            }
            int fout = -1, fin = -1; //maybe don't close (i dont know)
            if (output_exists)
            {
                fout = open(output_name, O_WRONLY | O_TRUNC | O_CREAT | O_CLOEXEC, S_IREAD|S_IWRITE);
                if (fout == -1) {perror("open output file failed"); return 1;}
                dup2(fout, STDOUT_FILENO);
            }
            if (input_exists)
            {
               fin = open(output_name, O_RDONLY | O_CLOEXEC);
               if (fin == -1) {perror("open input file failed"); return 1;}
                dup2(fin, STDIN_FILENO);

            }
            isredir = false; //after processing consider it simple command
            if (fork() == 0)
            {
                exec_command();
            }
            else
            {
                wait(nullptr);
                close(fout);
                close(fin);
            }
            return 0;
        }
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

private:
    bool isredir = true;
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
    void expand_reg_expr()
    {
        return;
    }
    //split args on {command}, {input_files} and {output_files}
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
void print_enter_line()
{
    uid_t euid = geteuid();
    char *dir;
    //cout << euid << " :asdfasfd" << endl;
    if (euid == 0)
    {
        cout << get_dir() << "!";
    }
    else
    {
        cout << get_dir() << ">";
    }
}
int exec_conveyer(vector<Command> &commands)
{
    if (commands.empty() || commands[0].is_empty())
    {
        return 0;
    }
    int stored_stdin = dup(STDIN_FILENO);
    int stored_stdout = dup(STDOUT_FILENO);
    if (commands[0].is_time())
    {
        struct rusage usage;
        struct timeval start_real_time, stop_real_time;
        gettimeofday(&start_real_time, NULL);
        ////////
        commands[0].delete_time();
        exec_conveyer(commands);
        gettimeofday(&stop_real_time, NULL);
        getrusage(RUSAGE_CHILDREN, &usage);
        auto real_time_sec = difftime(stop_real_time.tv_sec, start_real_time.tv_sec);
        auto real_time_usec = difftime(stop_real_time.tv_usec, start_real_time.tv_usec);
        cerr << "real: " << real_time_sec << "." << setfill('0') << setw(3) << real_time_usec << endl
             << "system: " << usage.ru_stime.tv_sec << "." << setfill('0') << setw(3) << usage.ru_stime.tv_usec << endl
             << "user: " << usage.ru_utime.tv_sec << "." << setfill('0') << setw(3) << usage.ru_utime.tv_usec << endl;
    } else if (commands.size() == 1)
    {
        if (commands[0].is_cd())
        {
            commands[0].exec_command();
            return 0;
        }
        if (commands[0].is_redir())
        {
            commands[0].exec_command(); // in this case fork will be inside
        }
        else if (fork() == 0)
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
        }
    }
     dup2(stored_stdin, STDIN_FILENO);
     dup2(stored_stdout, STDOUT_FILENO);
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
