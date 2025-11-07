/*
 * File: file_explorer.cpp
 * Author: Diksha Jethwa
 * Course: Linux Operating System (Assignment 1)
 * Description:
 *   Console-based File Explorer Application in C++.
 *   Allows navigation, manipulation, and search of files/directories
 *   using Linux system calls and C++17 filesystem library.
 */

#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <fstream>
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <ctime>

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>

#include <filesystem>
namespace fs = std::filesystem;

using std::string;
using std::cout;
using std::cin;
using std::endl;

// === ANSI COLOR CODES ===
#define RESET       "\033[0m"
#define BOLD        "\033[1m"
#define RED         "\033[31m"
#define GREEN       "\033[32m"
#define YELLOW      "\033[33m"
#define BLUE        "\033[34m"
#define CYAN        "\033[36m"
#define WHITE       "\033[37m"
#define BRIGHT_GREEN "\033[92m"

static string currentWorkingDir() {
    char buf[4096];
    if (getcwd(buf, sizeof(buf)) != nullptr) return string(buf);
    return string(".");
}

static string formatPermissions(mode_t mode) {
    string s;
    s += (S_ISDIR(mode) ? 'd' : '-');
    s += (mode & S_IRUSR) ? 'r' : '-';
    s += (mode & S_IWUSR) ? 'w' : '-';
    s += (mode & S_IXUSR) ? 'x' : '-';
    s += (mode & S_IRGRP) ? 'r' : '-';
    s += (mode & S_IWGRP) ? 'w' : '-';
    s += (mode & S_IXGRP) ? 'x' : '-';
    s += (mode & S_IROTH) ? 'r' : '-';
    s += (mode & S_IWOTH) ? 'w' : '-';
    s += (mode & S_IXOTH) ? 'x' : '-';
    return s;
}

static string fileOwnerUidUidToName(uid_t uid) {
    struct passwd *pw = getpwuid(uid);
    if (pw) return string(pw->pw_name);
    return std::to_string(uid);
}

static string fileGroupGidToName(gid_t gid) {
    struct group *gr = getgrgid(gid);
    if (gr) return string(gr->gr_name);
    return std::to_string(gid);
}

void listFiles(const string &path) {
    cout << BOLD << YELLOW << "Listing: " << path << RESET << "\n\n";
    DIR *dir = opendir(path.c_str());
    if (!dir) {
        cout << RED << "Failed to open directory: " << strerror(errno) << RESET << "\n";
        return;
    }

    struct dirent *entry;
    std::vector<string> names;
    while ((entry = readdir(dir)) != nullptr) {
        names.push_back(entry->d_name);
    }
    closedir(dir);
    std::sort(names.begin(), names.end());

    cout << BOLD << std::left
         << std::setw(30) << "Name"
         << std::setw(12) << "Type"
         << std::setw(12) << "Size"
         << std::setw(12) << "Perms"
         << std::setw(12) << "Owner"
         << std::setw(12) << "Group"
         << "Modified" << RESET << "\n";
    cout << std::string(100, '-') << "\n";

    for (const auto &name : names) {
        struct stat st;
        string full = (path == "." ? name : (path + "/" + name));
        if (lstat(full.c_str(), &st) != 0) {
            cout << RED << name << " (error reading)" << RESET << "\n";
            continue;
        }

        string type = S_ISDIR(st.st_mode) ? "Directory" :
                      S_ISLNK(st.st_mode) ? "Symlink" : "File";

        // Apply color based on type
        string color;
        if (S_ISDIR(st.st_mode)) color = BLUE;
        else if (S_ISLNK(st.st_mode)) color = CYAN;
        else color = WHITE;

        char timebuf[64];
        struct tm *tm_info = localtime(&st.st_mtime);
        strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M", tm_info);

        cout << color << std::left << std::setw(30) << name << RESET
             << std::setw(12) << type
             << std::setw(12) << st.st_size
             << std::setw(12) << formatPermissions(st.st_mode)
             << std::setw(12) << fileOwnerUidUidToName(st.st_uid)
             << std::setw(12) << fileGroupGidToName(st.st_gid)
             << timebuf << "\n";
    }
}

bool changeDirectory(string &cwd, const string &target) {
    string newPath = target;
    if (target == "~") newPath = getenv("HOME") ? getenv("HOME") : "/";
    if (chdir(newPath.c_str()) == 0) {
        cwd = currentWorkingDir();
        cout << BRIGHT_GREEN << "Changed to: " << cwd << RESET << "\n";
        return true;
    } else {
        cout << RED << "chdir failed: " << strerror(errno) << RESET << "\n";
        return false;
    }
}

bool createFile(const string &filename) {
    std::ofstream ofs(filename, std::ios::app);
    if (!ofs) {
        cout << RED << "Failed to create file: " << strerror(errno) << RESET << "\n";
        return false;
    }
    ofs.close();
    cout << BRIGHT_GREEN << "Created: " << filename << RESET << "\n";
    return true;
}

bool deletePath(const string &path) {
    std::error_code ec;
    if (fs::is_directory(path, ec)) {
        if (fs::remove(path, ec)) {
            cout << BRIGHT_GREEN << "Deleted directory: " << path << RESET << "\n";
            return true;
        }
        cout << RED << "Failed to remove directory: " << ec.message() << RESET << "\n";
        return false;
    } else {
        if (fs::remove(path, ec)) {
            cout << BRIGHT_GREEN << "Deleted file: " << path << RESET << "\n";
            return true;
        }
        cout << RED << "Failed to remove file: " << ec.message() << RESET << "\n";
        return false;
    }
}

bool copyFile(const string &src, const string &dst) {
    try {
        if (fs::is_directory(src)) {
            fs::copy(src, dst, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
        } else {
            fs::copy_file(src, dst, fs::copy_options::overwrite_existing);
        }
        cout << BRIGHT_GREEN << "Copied to: " << dst << RESET << "\n";
        return true;
    } catch (const fs::filesystem_error &e) {
        cout << RED << "Copy failed: " << e.what() << RESET << "\n";
        return false;
    }
}

bool moveFile(const string &src, const string &dst) {
    try {
        fs::rename(src, dst);
        cout << BRIGHT_GREEN << "Moved to: " << dst << RESET << "\n";
        return true;
    } catch (const fs::filesystem_error &e) {
        cout << RED << "Rename failed: " << e.what() << RESET << "\n";
        if (copyFile(src, dst)) {
            if (deletePath(src)) return true;
        }
        return false;
    }
}

void searchFiles(const string &root, const string &pattern) {
    cout << CYAN << "Searching for \"" << pattern << "\" under " << root << " ...\n" << RESET;
    try {
        for (auto &p : fs::recursive_directory_iterator(root)) {
            if (p.path().filename().string().find(pattern) != string::npos) {
                cout << GREEN << p.path().string() << RESET << "\n";
            }
        }
    } catch (const fs::filesystem_error &e) {
        cout << RED << "Search error: " << e.what() << RESET << "\n";
    }
}

bool changePermission(const string &path, const string &modeStr) {
    try {
        unsigned long mode = std::stoul(modeStr, nullptr, 8);
        if (chmod(path.c_str(), static_cast<mode_t>(mode)) != 0) {
            cout << RED << "chmod failed: " << strerror(errno) << RESET << "\n";
            return false;
        }
        cout << BRIGHT_GREEN << "Permissions changed for " << path << RESET << "\n";
        return true;
    } catch (...) {
        cout << RED << "Invalid mode. Provide octal like 755 or 0755" << RESET << "\n";
        return false;
    }
}

void printHelp() {
    cout << "\n" << BOLD << YELLOW << "--- Commands (Menu) ---" << RESET << "\n";
    cout << GREEN << "1 " << RESET << "- List files in current directory\n";
    cout << GREEN << "2 " << RESET << "- Change directory (cd)\n";
    cout << GREEN << "3 " << RESET << "- Create file\n";
    cout << GREEN << "4 " << RESET << "- Delete file/directory (rm)\n";
    cout << GREEN << "5 " << RESET << "- Copy file/directory\n";
    cout << GREEN << "6 " << RESET << "- Move/Rename file/directory\n";
    cout << GREEN << "7 " << RESET << "- Search (recursive)\n";
    cout << GREEN << "8 " << RESET << "- Change permissions (chmod)\n";
    cout << GREEN << "9 " << RESET << "- Show current working directory\n";
    cout << GREEN << "10" << RESET << "- Detailed list (ls -la style)\n";
    cout << GREEN << "0 " << RESET << "- Exit\n";
}

void detailedList(const string &path) {
    listFiles(path);
}

int main() {
    string cwd = currentWorkingDir();
    cout << BOLD << YELLOW << "Simple Linux File Explorer (C++)" << RESET << "\n";
    cout << CYAN << "Working directory: " << cwd << RESET << "\n";
    printHelp();

    while (true) {
        cout << "\n" << BOLD << GREEN << "[" << cwd << "]> " << RESET;
        int choice = -1;
        if (!(cin >> choice)) {
            cin.clear();
            string throwaway;
            getline(cin, throwaway);
            cout << RED << "Invalid input" << RESET << "\n";
            continue;
        }
        string a;
        getline(cin, a); // consume rest of line

        if (choice == 0) {
            cout << YELLOW << "Exiting. Bye!" << RESET << "\n";
            break;
        }

        switch (choice) {
            case 1:
                listFiles(cwd);
                break;
            case 2: {
                cout << "Enter directory: ";
                string dir; getline(cin, dir);
                if (!dir.empty()) changeDirectory(cwd, dir);
                break;
            }
            case 3: {
                cout << "Enter filename: ";
                string file; getline(cin, file);
                createFile(file);
                break;
            }
            case 4: {
                cout << "Enter path to delete: ";
                string target; getline(cin, target);
                deletePath(target);
                break;
            }
            case 5: {
                cout << "Enter source path: ";
                string src; getline(cin, src);
                cout << "Enter destination path: ";
                string dst; getline(cin, dst);
                copyFile(src, dst);
                break;
            }
            case 6: {
                cout << "Enter source path: ";
                string src; getline(cin, src);
                cout << "Enter destination path: ";
                string dst; getline(cin, dst);
                moveFile(src, dst);
                break;
            }
            case 7: {
                cout << "Enter search root: ";
                string root; getline(cin, root);
                if (root.empty()) root = cwd;
                cout << "Enter pattern: ";
                string pattern; getline(cin, pattern);
                searchFiles(root, pattern);
                break;
            }
            case 8: {
                cout << "Enter path: ";
                string path; getline(cin, path);
                cout << "Enter mode (e.g. 755): ";
                string mode; getline(cin, mode);
                changePermission(path, mode);
                break;
            }
            case 9:
                cout << CYAN << "Current directory: " << cwd << RESET << "\n";
                break;
            case 10:
                detailedList(cwd);
                break;
            default:
                printHelp();
                break;
        }
    }
    return 0;
}
