#include <bits/types/sig_atomic_t.h>
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/rtc.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cassert>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
using namespace std;

#define HOME_PATH getenv("HOME")

string get_dir() {
  char *dir;
  dir = get_current_dir_name();
  string str(dir);
  free(dir);
  return str;
}

vector<string> split_line_by_char(const string &command_line,
                                  const char delim) {
  vector<string> splitted_line;
  string token;
  int cur_ptr = 0, prev_ptr = 0;
  while ((cur_ptr = command_line.find(delim, prev_ptr)) != string::npos) {
    token = command_line.substr(prev_ptr, cur_ptr - prev_ptr);
    splitted_line.push_back(token);
    prev_ptr = cur_ptr + 1;
  }
  splitted_line.push_back(
      command_line.substr(prev_ptr, command_line.length() - prev_ptr));
  return splitted_line;
}
void print_enter_line() {
  uid_t euid = geteuid();
  char *dir;
  // cout << euid << " :asdfasfd" << endl;
  if (euid == 0) {
    cout << get_dir() << "!";
  } else {
    cout << get_dir() << ">";
  }
}

class Command {
public:
  vector<string> all_args;
  vector<string> command_args;      // "command < file1 > file2", command empty
                                    //  in case no redirection
  vector<pair<string, bool>> files; // file args: 0 - if input, 1 - if output
  int fd_in = -1, fd_out = -1;
  Command(const string &input) {
    split_line(input);
    split_redir();
    expand_reg_expr_all(command_args);
  }
  ~Command() {}
  void print() {
    for (auto &i : all_args) {
      cout << cout.width(2) << i;
    }
    cout << endl;
  }
  bool is_empty() { return command_args.empty(); }
  bool is_redir() { return isredir; }
  bool is_cd() { return is_empty() ? false : command_args[0] == "cd"; }
  bool is_time() { return is_empty() ? false : command_args[0] == "time"; }
  bool is_pwd() { return is_empty() ? false : command_args[0] == "pwd"; }
  void delete_time() {
    if (is_time()) {
      command_args.erase(command_args.begin());
    }
    return;
  }
  int exec_command() {
    if(dont_executable) {
      return 0;
    }
    if (is_empty()) {
      perror("is_empty");
      return 0;
    }
    if (is_cd()) {
      if (command_args.size() == 1) {
        exec_cd(HOME_PATH);
      } else if (command_args.size() == 2) {
        exec_cd(command_args[1]);
      } else {
        for (auto &i : command_args) {
          cout << i << endl;
        }
        perror("too many arguments\n");
      }
    } else if (is_pwd()) {
      exec_pwd();
    } else {
      exec_bash_command(command_args);
    }
    return 0;
  }
  // do redirection stdio to files, open file descriptors
  int prepare_command_if_redir() {
    if (!is_redir()) {
      return 0;
    }
    bool input_exists = false, output_exists = false;
    string input_name, output_name;
    for (int i = files.size() - 1; i >= 0; i--) {
      if (files[i].second == 0 && !input_exists) {
        input_exists = true;
        input_name = files[i].first;
      } else if (files[i].second == 1 && !output_exists) {
        output_exists = true;
        output_name = files[i].first;
      }
    }
    fd_out = -1, fd_in = -1;
    if (output_exists) {
      fd_out = open(output_name.c_str(), O_WRONLY | O_TRUNC | O_CREAT, 0644);
      if (fd_out == -1) {
        perror("open output file failed");
        dont_executable = true;
        return 1;
      }
      dup2(fd_out, STDOUT_FILENO);
    }
    if (input_exists) {
      fd_in = open(input_name.c_str(), O_RDONLY);
      if (fd_in == -1) {
        perror("open input file failed");
        dont_executable = true;
        return 1;
      }
      dup2(fd_in, STDIN_FILENO);
    }
    return 0;
  }

private:
  bool isredir = true;
  bool dont_executable = false;
  vector<string> path;
  // split args by spaces
  void split_line(const string &command_line) {
    auto first = command_line.begin();
    while (first != command_line.end()) {
      first = find_if_not(first, command_line.end(), ::isspace);
      auto last = find_if(first, command_line.end(), ::isspace);
      if (first != command_line.end()) {
        all_args.push_back(string(first, last));
        first = last;
      }
    }
  }
  void exec_pwd() {
    cout << get_dir() << endl;
    exit(0);
  }
  void exec_bash_command(const vector<string> &args) {
    vector<char *> v;
    for (size_t i = 0; i < args.size(); i++) {
      v.push_back((char *)args[i].c_str());
    }
    v.push_back(NULL);
    prctl(PR_SET_PDEATHSIG, SIGINT); // exit process when parent dies
    execvp(v[0], &v[0]);
    cerr << "Execution error" << endl;
    exit(1);
  }
  void exec_cd(const string &arg) {
    int code = chdir(arg.c_str());
    if (code == -1) {
      cerr << "No such file or directory" << endl;
    }
  }
  bool is_reg_expr(const string &arg) {
    if (arg.find('*') != string::npos || arg.find('?') != string::npos) {
      return true;
    }
    return false;
  }
  void expand_reg_expr_all(vector<string> &expr_args) {
    for (int i = 0; i < expr_args.size();
         i++) { //цикл по всем аргументам команды
      if (!is_reg_expr(expr_args[i])) {
        continue;
      }
      stringstream result;
      vector<string> arg_split = split_line_by_char(expr_args[i], '/');
      vector<string> expanded_args = expand_reg_expr_arg(arg_split);
      if (expanded_args.empty()) {
        continue;
      }
      expr_args.insert(expr_args.begin() + i, expanded_args.begin(),
                       expanded_args.end());
      i += expanded_args.size();
      expr_args.erase(expr_args.begin() + i);
    }
  }
  //на вход подается регулярное выражение(путь) разделенное по слэшу
  vector<string> expand_reg_expr_arg(vector<string> &args) {
    vector<string> result;
    string path;
    bool flag = false;
    if (args[0].empty()) {
      path = "/";
      flag = true;
    } else {
      path = get_dir() + "/";
    }
    result.push_back("");
    bool is_dir = false;
    if (args[args.size() - 1].empty()) {
      is_dir = true;
    }
    vector<string> reverse_args;
    for (auto it = args.rbegin(); it != args.rend(); it++) {
      if (it->empty()) {
        continue;
      }
      reverse_args.push_back(*it);
    }
    int args_ptr = reverse_args.size() - 1;
    while (args_ptr >= 0) {
      vector<string> temp;
      for (string &i : result) {
        if (flag) {
          i += '/';
        }
        DIR *dir = opendir((path + i).c_str());
        if (dir != nullptr) {
          for (dirent *d = readdir(dir); d != nullptr; d = readdir(dir)) {
            if (d->d_name[0] == '.') {
              continue;
            }
            if (args_ptr > 0) {
              if (d->d_type == DT_DIR &&
                  match_reg_expr(d->d_name, reverse_args[args_ptr])) {
                temp.push_back(i + d->d_name);
              }
            } else {
              if ((d->d_type == DT_DIR || (d->d_type == DT_REG && !is_dir)) &&
                  match_reg_expr(d->d_name, reverse_args[args_ptr])) {
                temp.push_back(i + d->d_name);
              }
            }
          }
        }
        closedir(dir);
      }
      result = temp;
      args_ptr--;
      flag = true;
    }
    if (result.empty()) {
      return {};
    }
    return result;
  }
  bool match_reg_expr(string word_str, string expr_str) {
    const char *expr = expr_str.c_str();
    const char *word = word_str.c_str();
    const char *prev_word = 0, *prev_expr;
    while (1) {
      if (*expr == '*') {
        prev_word =
            word; //указывает на часть слова, которую уже поглотила звезда
        prev_expr = ++expr; //указывает на последнюю рассматриваемую
                            //звезду(на символ после нее)
      } else if (!*word) {
        return !*expr;
      } else if (*word == *expr ||
                 *expr == '?') { //сдвигаем указатели, если найдено совпадение
        ++word;
        ++expr;
      } else if (prev_word) { //если произошла неудача - поглащаем еще один
                              //символ
        word = ++prev_word;
        expr = prev_expr;
      } else { //если звезда произошла неудача, и при этом звезда поглотила все
               //слово
        return false;
      }
    }
  }
  // split args into {command}, {input_files} and {output_files}
  void split_redir() {
    auto less_it = find(all_args.begin(), all_args.end(), "<");
    auto more_it = find(all_args.begin(), all_args.end(), ">");
    if (less_it < more_it) {
      command_args.assign(all_args.begin(), less_it);
    } else if (less_it > more_it) {
      command_args.assign(all_args.begin(), more_it);
    } else {
      command_args = all_args;
      isredir = false;
      return;
    }
    while (less_it != all_args.end() || more_it != all_args.end()) {
      if (less_it < all_args.end() - 1) {
        files.push_back(make_pair(*(less_it + 1), 0));
      }
      if (more_it < all_args.end() - 1) {
        files.push_back(make_pair(*(more_it + 1), 1));
      }
      if (less_it != all_args.end()) {
        less_it++;
      }
      if (more_it != all_args.end()) {
        more_it++;
      }
      less_it = find(less_it, all_args.end(), "<");
      more_it = find(more_it, all_args.end(), ">");
    }
    return;
  }
};

class Conveyer {
public:
  vector<Command> commands;
  Conveyer(string &input) {
    vector<string> splitted_conveyer = split_line_by_char(input, '|');
    for (const auto &i : splitted_conveyer) {
      commands.push_back(i);
    }
    // remember original std file descriptors
    stored_stdin = dup(STDIN_FILENO);
    stored_stdout = dup(STDOUT_FILENO);
  }
  ~Conveyer() {
    dup2(stored_stdin, STDIN_FILENO);
    dup2(stored_stdout, STDOUT_FILENO);
    close(stored_stdin);
    close(stored_stdout);
  }
  int exec_conveyer() {
    if (commands.empty() || commands[0].is_empty()) {
      return 0;
    }
    if (commands[0].is_time()) {
      commands[0].delete_time();
      struct rusage start, stop;
      struct timeval start_real_time, stop_real_time;
      getrusage(RUSAGE_CHILDREN, &start);
      gettimeofday(&start_real_time, NULL);
      ////////
      exec_conveyer();
      ////////
      gettimeofday(&stop_real_time, NULL);
      getrusage(RUSAGE_CHILDREN, &stop);
      auto real_time_sec = stop_real_time.tv_sec - start_real_time.tv_sec;
      auto real_time_usec = stop_real_time.tv_usec - start_real_time.tv_usec;
      cerr << "real: " << real_time_sec << "." << setfill('0') << setw(3)
           << real_time_usec << endl
           << "system: " << stop.ru_stime.tv_sec - start.ru_stime.tv_sec << "."
           << setfill('0') << setw(3)
           << stop.ru_stime.tv_usec - start.ru_stime.tv_usec << endl
           << "user: " << stop.ru_utime.tv_sec - start.ru_utime.tv_sec << "."
           << setfill('0') << setw(3)
           << stop.ru_utime.tv_usec - start.ru_utime.tv_usec << endl;
      return 0;
    }
    if (commands[0].is_cd()) {
      commands[0].exec_command();
      return 0;
    }
    int cur_fd = 0, prev_fd = -1;
    vector<int[2]> pipes(commands.size() - 1);
    for (auto &fd : pipes) {
      if (pipe2(fd, O_CLOEXEC) != 0) {
        perror("pipe\n");
      }
    }
    for (int i = 0; i < commands.size(); i++, cur_fd++, prev_fd++) {
      int code = commands[i].prepare_command_if_redir();
      pid_t pid = fork();
      if (pid == 0) {
        if (commands.size() == 1) {
          commands[0].exec_command();
        } else if (i == 0) {
          dup2(pipes[0][1], STDOUT_FILENO);
          close(pipes[0][0]);
          commands[i].exec_command();
        } else if ((i > 0) && (i < commands.size() - 1)) {
          dup2(pipes[prev_fd][0], STDIN_FILENO);
          close(pipes[prev_fd][1]);
          dup2(pipes[cur_fd][1], STDOUT_FILENO);
          close(pipes[cur_fd][0]);
          commands[i].exec_command();
        } else if (i == commands.size() - 1) {
          dup2(pipes[prev_fd][0], STDIN_FILENO);
          close(pipes[prev_fd][1]);
          commands[i].exec_command();
        }
      } else {
        if (i == commands.size() - 1) {
          for (auto &i : pipes) {
            if (i[0] != -1) {
              close(i[0]);
              close(i[1]);
            }
          }
          for (int i = 0; i < commands.size(); i++) {
            wait(NULL);
          }
          //в конце закрываем трубы в родителе и ждем всех детей//
        }
        // close if opened
        if (commands[i].fd_in != -1) {
          close(commands[i].fd_in);
          commands[i].fd_in = -1;
        }
        if (commands[i].fd_out != -1) {
          close(commands[i].fd_out);
          commands[i].fd_out = -1;
        }
      }
    }
    return 0;
  }

private:
  int stored_stdin, stored_stdout;
};

sig_atomic_t flag = 0;
void sig_handler(int signal) {
  if (signal == 2) {
    flag = 1;
  }
}
int main() {
  while (true) {
    flag = 0;
    signal(SIGINT, sig_handler);
    print_enter_line();
    try {
      string input;
      if (!getline(cin, input)) {
        throw string("EOF");
      }
      if (flag) {
        continue;
      }
      Conveyer conveyer(input);
      conveyer.exec_conveyer();
    } catch (string a) {
      cerr << a << " was occured" << endl;
      break;
    }
  }
}
