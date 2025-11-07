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
    cout << "Listing: " << path << "\n\n";
    DIR *dir = opendir(path.c_str());
    if (!dir) {
        cout << "Failed to open directory: " << strerror(errno) << "\n";
        return;
    }

    struct dirent *entry;
    std::vector<string> names;
    while ((entry = readdir(dir)) != nullptr) {
        names.push_back(entry->d_name);
    }
    closedir(dir);
    std::sort(names.begin(), names.end());

    cout << std::left << std::setw(30) << "Name"
         << std::setw(12) << "Type"
         << std::setw(12) << "Size"
         << std::setw(12) << "Perms"
         << std::setw(12) << "Owner"
         << std::setw(12) << "Group"
         << " Modified\n";
    cout << std::string(100, '-') << "\n";

    for (const auto &name : names) {
        struct stat st;
        string full = (path == "." ? name : (path + "/" + name));
        if (lstat(full.c_str(), &st) != 0) {
            cout << name << "\n";
            continue;
        }

        string type = S_ISDIR(st.st_mode) ? "Directory" :
                      S_ISLNK(st.st_mode) ? "Symlink" : "File";

        char timebuf[64];
        struct tm *tm_info = localtime(&st.st_mtime);
        strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M", tm_info);

        cout << std::left << std::setw(30) << name
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
        return true;
    } else {
        cout << "chdir failed: " << strerror(errno) << "\n";
        return false;
    }
}

bool createFile(const string &filename) {
    std::ofstream ofs(filename, std::ios::app);
    if (!ofs) {
        cout << "Failed to create file: " << strerror(errno) << "\n";
        return false;
    }
    ofs.close();
    return true;
}

bool deletePath(const string &path) {
    std::error_code ec;
    if (fs::is_directory(path, ec)) {
        if (fs::remove(path, ec)) return true;
        cout << "Failed to remove directory: " << ec.message() << "\n";
        return false;
    } else {
        if (fs::remove(path, ec)) return true;
        cout << "Failed to remove file: " << ec.message() << "\n";
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
        return true;
    } catch (const fs::filesystem_error &e) {
        cout << "Copy failed: " << e.what() << "\n";
        return false;
    }
}

bool moveFile(const string &src, const string &dst) {
    try {
        fs::rename(src, dst);
        return true;
    } catch (const fs::filesystem_error &e) {
        cout << "Rename failed: " << e.what() << "\n";
        if (copyFile(src, dst)) {
            if (deletePath(src)) return true;
        }
        return false;
    }
}

void searchFiles(const string &root, const string &pattern) {
    cout << "Searching for \"" << pattern << "\" under " << root << " ...\n";
    try {
        for (auto &p : fs::recursive_directory_iterator(root)) {
            if (p.path().filename().string().find(pattern) != string::npos) {
                cout << p.path().string() << "\n";
            }
        }
    } catch (const fs::filesystem_error &e) {
        cout << "Search error: " << e.what() << "\n";
    }
}

bool changePermission(const string &path, const string &modeStr) {
    try {
        unsigned long mode = std::stoul(modeStr, nullptr, 8);
        if (chmod(path.c_str(), static_cast<mode_t>(mode)) != 0) {
            cout << "chmod failed: " << strerror(errno) << "\n";
            return false;
        }
        return true;
    } catch (...) {
        cout << "Invalid mode. Provide octal like 755 or 0755\n";
        return false;
    }
}

void printHelp() {
    cout << "\n--- Commands (Menu) ---\n";
    cout << "1  - List files in current directory\n";
    cout << "2  - Change directory (cd)\n";
    cout << "3  - Create file\n";
    cout << "4  - Delete file/directory (rm)\n";
    cout << "5  - Copy file/directory\n";
    cout << "6  - Move/Rename file/directory\n";
    cout << "7  - Search (recursive)\n";
    cout << "8  - Change permissions (chmod)\n";
    cout << "9  - Show current working directory\n";
    cout << "10 - Detailed list (ls -la style)\n";
    cout << "0  - Exit\n";
}

void detailedList(const string &path) {
    listFiles(path);
}

int main() {
    string cwd = currentWorkingDir();
    cout << "Simple Linux File Explorer (C++)\n";
    cout << "Working directory: " << cwd << "\n";
    printHelp();

    while (true) {
        cout << "\n[" << cwd << "]> ";
        int choice = -1;
        if (!(cin >> choice)) {
            cin.clear();
            string throwaway;
            getline(cin, throwaway);
            cout << "Invalid input\n";
            continue;
        }
        string a;
        getline(cin, a); // consume rest of line

        if (choice == 0) {
            cout << "Exiting. Bye!\n";
            break;
        }

        switch (choice) {
            case 1:
                listFiles(cwd);
                break;
            case 2: {
                cout << "Enter directory: ";
                string dir; getline(cin, dir);
                if (dir.empty()) break;
                if (changeDirectory(cwd, dir))
                    cout << "Changed to: " << cwd << "\n";
                break;
            }
            case 3: {
                cout << "Enter filename: ";
                string file; getline(cin, file);
                if (createFile(file))
                    cout << "Created: " << file << "\n";
                break;
            }
            case 4: {
                cout << "Enter path to delete: ";
                string target; getline(cin, target);
                if (deletePath(target))
                    cout << "Deleted: " << target << "\n";
                break;
            }
            case 5: {
                cout << "Enter source path: ";
                string src; getline(cin, src);
                cout << "Enter destination path: ";
                string dst; getline(cin, dst);
                if (copyFile(src, dst))
                    cout << "Copied to: " << dst << "\n";
                break;
            }
            case 6: {
                cout << "Enter source path: ";
                string src; getline(cin, src);
                cout << "Enter destination path: ";
                string dst; getline(cin, dst);
                if (moveFile(src, dst))
                    cout << "Moved to: " << dst << "\n";
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
                if (changePermission(path, mode))
                    cout << "Permissions changed for " << path << "\n";
                break;
            }
            case 9:
                cout << "Current directory: " << cwd << "\n";
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
