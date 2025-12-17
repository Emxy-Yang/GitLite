

#ifndef GITLITE_REFMANAGER_HPP
#define GITLITE_REFMANAGER_HPP

#include<string>
#include "Objects.hpp"

class RefManager {
private:
    //root path
    const std::string MASTER_DIR = ".gitlite/refs/heads";
    const std::string HEAD_FILE = ".gitlite/HEAD";

public:
    void initManager(Commit& init_commit);
    std::string getBranchPath(const std::string& branchName) const;
    void updateRef(const std::string& refName, const std::string& newHash);

    void updateRemoteRef(const std::string &remoteName, const std::string &remoteBranchName,
                         const std::string &newHash);

    std::string getRemoteTrackingBranchPath(const std::string &remoteTrackingName) const;

    // 解析 HEAD
    std::string resolveHead();

    std::string getCurrentBranchName();

    void createBranch(const std::string& branchName);

    //  rm-branch
    void removeBranch(const std::string& branchName);

    std::vector<std::string> getAllBranchNames() const;
};


class RemoteRefManager {
private:
    std::string remote_root_dir;
    std::string getRefPath(const std::string& refName) const;
public:
    explicit RemoteRefManager(const std::string& gitlite_root_dir);

    std::string resolveRef(const std::string& refName) const;

    void updateRef(const std::string& refName, const std::string& newHash);
};

class Ref {
private:
    std::string name;
    std::string target;
    bool symbolic;
public:
    Ref(std::string n, std::string t, bool isSymbolic = false)
        : name(std::move(n)), target(std::move(t)), symbolic(isSymbolic) {}

    std::string getName() const { return name; }
    std::string getTarget() const { return target; }

    std::string serialize() const ;

};



#endif //GITLITE_REFMANAGER_HPP