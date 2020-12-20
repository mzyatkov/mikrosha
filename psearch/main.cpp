#include <dirent.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
using namespace std;

class Task {
   public:
    queue<string> task_pool;
    mutex task_guard;
    bool ls_done = false;
    Task(string path, bool in_one_dir) {
        ls(path, !in_one_dir);
        ls_done = true;
    }
    void ls(string path, bool recursive) {
        DIR *dir = opendir(path.c_str());
        if (dir == nullptr) return;
        for (dirent *d = readdir(dir); d != nullptr; d = readdir(dir)) {
            if (recursive && d->d_type == DT_DIR) {
                if (d->d_name[0] == '.') continue;
                ls(path + string("/") + d->d_name, true);
            } else if (d->d_type == DT_REG) {
                task_guard.lock();
                task_pool.push(path + "/" + d->d_name);
                task_guard.unlock();
            }
        }
        closedir(dir);
    }
};

int KMP(const string &input, const string &pattern) {
    vector<int> prefunc(pattern.length());

    prefunc[0] = 0;
    for (int k = 0, i = 1; i < pattern.length(); ++i) {
        while ((k > 0) && (pattern[i] != pattern[k]))
            k = prefunc[k - 1];

        if (pattern[i] == pattern[k])
            k++;

        prefunc[i] = k;
    }

    for (int k = 0, i = 0; i < input.length(); ++i) {
        while ((k > 0) && (pattern[k] != input[i])) {
            k = prefunc[k - 1];
        }
        if (pattern[k] == input[i]) {
            k++;
        }
        if (k == pattern.length()) {
            return (i - pattern.length() + 1);  //первое вхождение
        }
    }

    return -1;
}

void file_search(string *path, string *pattern, mutex *output_guard) {
    ifstream fin(*path);
    if (fin) {
        string str;
        for (int i = 0; getline(fin, str); i++) {
            int ptr;
            if ((ptr = KMP(str, *pattern)) != -1) {
                output_guard->lock();
                printf("file: %s str_num: %d str: %s\n", path->c_str(), i, str.c_str());
                output_guard->unlock();
            }
        }
    } else {
        //cout << "No such file" << endl;
    }
    return;
}

string get_dir() {
    char *dir;
    dir = get_current_dir_name();
    string str(dir);
    free(dir);
    return str;
}

int main(int argc, char **argv) {
    int thread_num = 1;
    string dir_name = get_dir();
    string pattern;
    bool search_in_one_dir = false;
    vector<string> args;
    for (int i = 1; i < argc; i++) {
        args.push_back(argv[i]);
    }
    //обработка аргументов командной строки
    for (auto &str : args) {
        if (str[0] == '-') {
            if (str == "-n") {
                search_in_one_dir = true;
            } else if (str.length() == 3 && str[0] == '-' && str[1] == 't') {
                thread_num = str[2] - '0';
                if (thread_num < 0 || thread_num > 9) {
                    perror("wrong argument");
                    exit(1);
                }
            }
        } else {
            if (pattern.empty()) {
                pattern = str;
            } else {
                dir_name = str;
            }
        }
    }
    if (pattern.empty()) {
        perror("empty query");
        exit(1);
    }
    ///////////////////////////////////
    pattern = "\1";

    Task task(dir_name, search_in_one_dir);
    mutex output_guard;
    vector<thread> threads(thread_num);
    int ptr = 0;
    vector<string> targets(thread_num);
    do {
        while ((!task.task_pool.empty() && ptr != thread_num)) {
            task.task_guard.lock();
            targets[ptr] = task.task_pool.front();
            task.task_pool.pop();
            task.task_guard.unlock();
            
            threads[ptr] = thread(file_search, &targets[ptr], &pattern, &output_guard);
            ptr++;
        }
        if (ptr == thread_num) {
            for (int i = 0; i < thread_num; i++) {
                threads[i].join();
            }
            ptr = 0;
        } else if (task.ls_done) {
            for (int i = 0; i < ptr; i++) {
                threads[i].join();
            }
        }
    } while (!task.ls_done || !task.task_pool.empty());

    return 0;
}