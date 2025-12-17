#ifndef GITLITE_REMOTEMANAGER_HPP
#define GITLITE_REMOTEMANAGER_HPP

#include <string>
#include <map>
#include <vector>
const std::string REMOTE_FILE = ".gitlite/remotes";

class RemoteManager {
public:
    RemoteManager();

    void addRemote(const std::string& name, std::string path);

    void rmRemote(const std::string& name);

    std::string getRemotePath(const std::string& name) const;

    bool remoteExists(const std::string& name) const;

private:
    std::map<std::string, std::string> remotes;

    void saveRemotes() const;

    void loadRemotes();

    std::string normalizePath(std::string path);
};

#endif

