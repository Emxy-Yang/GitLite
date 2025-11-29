

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

    // 解析 HEAD
    std::string resolveHead();

    // 获取当前分支名
    std::string getCurrentBranchName();

    //  创建分支
    void createBranch(const std::string& branchName);

    // 删除分支 (git rm-branch )
    void removeBranch(const std::string& branchName);
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