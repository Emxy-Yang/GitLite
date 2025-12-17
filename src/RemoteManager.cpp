//
// Created by Yang Jibin on 2025/12/14.
//

#include "RemoteManager.hpp"


#include "Utils.h" // 假设 Utils 包含读写和路径操作
#include "GitliteException.h" // 假设您有统一的异常处理

#include <sstream>
#include <stdexcept>
#include <algorithm>

RemoteManager::RemoteManager() {
    loadRemotes();
}

void RemoteManager::loadRemotes() {
    remotes.clear();
    if (!Utils::exists(REMOTE_FILE)) {
        return;
    }

    std::string content = Utils::readContentsAsString(REMOTE_FILE);
    std::stringstream ss(content);
    std::string line;

    // 文件格式：每行 [name] [path]
    while (std::getline(ss, line)) {
        if (line.empty()) continue;

        size_t first_space = line.find(' ');
        if (first_space == std::string::npos) {
            continue;
        }

        std::string name = line.substr(0, first_space);
        std::string path = line.substr(first_space + 1);

        path.erase(path.find_last_not_of(" \n\r\t") + 1);  //debug

        remotes[name] = path;
    }
}

void RemoteManager::saveRemotes() const {
    std::stringstream ss;
    for (const auto& pair : remotes) {
        // 格式：name path\n
        ss << pair.first << " " << pair.second << "\n";
    }
    Utils::writeContents(REMOTE_FILE, ss.str());
}


bool RemoteManager::remoteExists(const std::string& name) const {
    return remotes.count(name);
}

void RemoteManager::addRemote(const std::string& name, std::string path) {
    if (remoteExists(name)) {
        Utils::exitWithMessage("A remote with that name already exists.");
    }
    std::string normalized_path = path;
    remotes[name] = normalized_path;
    saveRemotes();
}

void RemoteManager::rmRemote(const std::string& name) {
    if (!remoteExists(name)) {
        Utils::exitWithMessage("A remote with that name does not exist.");
    }
    remotes.erase(name);
    saveRemotes();
}

std::string RemoteManager::getRemotePath(const std::string& name) const {
    auto it = remotes.find(name);
    if (it != remotes.end()) {
        return it->second;
    }
    return "";
}