#include "RefManager.hpp"

#include <utility>

#include "Utils.h"

void RefManager::initManager(Commit& init_commit) {
    Utils::writeContents(getBranchPath("master"),init_commit.get_hashid());
    Utils::writeContents(HEAD_FILE,"ref: refs/heads/master");
}

std::string RefManager::getBranchPath(const std::string& branchName) const {
    return Utils::join(MASTER_DIR, branchName);
}

void RefManager::updateRef(const std::string &refName, const std::string &newHash) {
    // check hash
    if (newHash.empty() || newHash.length() != 40) {
        Utils::exitWithMessage("Invalid hash provided for reference update.");
    }

    std::string refPath;

    //if ref is HEAD
    if (refName == "HEAD") {
        //read
        std::string headContent = Utils::readContentsAsString(HEAD_FILE);
        if (!headContent.empty() && headContent.back() == '\n') {
            headContent.pop_back();
        }

        const std::string refPrefix = "ref: ";

        if (headContent.substr(0, refPrefix.size()) == refPrefix) {
            //headContent = "ref: refs/heads/master"

            //get "refs/heads/master" and add ".gitlite"
            std::string symbolicRef = headContent.substr(refPrefix.size());
            refPath = Utils::join(".gitlite", symbolicRef);
        } else {
            //Detached HEAD state: HEAD file comtains only one hashid
            //此时，新的 Commit 直接覆盖 HEAD 文件本身
            refPath = HEAD_FILE;
        }
    }
    //refs/heads/branchName state
    else if (refName.substr(0, 11) == "refs/heads/") {
        refPath = Utils::join(".gitlite", refName);
    }
    else {
        Utils::exitWithMessage("Unsupported reference name: " + refName);
        return;
    }

    //write
    Utils::writeContents(refPath, newHash + "\n");
}

std::string RefManager::resolveHead() {
    std::string content = Utils::readContentsAsString(HEAD_FILE);

    if (!content.empty() && content.back() == '\n') content.pop_back();

    if (content.substr(0, 5) == "ref: ") {
        // HEAD 指向分支，读取该分支的 Hash
        std::string refPath = ".gitlite/" + content.substr(5);
        if (!Utils::exists(refPath)) return ""; // 空仓库情况
        std::string hash = Utils::readContentsAsString(refPath);

        if (!hash.empty() && hash.back() == '\n') hash.pop_back();
        return hash;
    } else {
        // Detached HEAD，直接返回 Hash
        return content;
    }
}

std::string RefManager::getCurrentBranchName() {
    std::string content = Utils::readContentsAsString(HEAD_FILE);
    if (content.substr(0, 5) == "ref: ") {
        // 提取 refs/heads/master 中的 master
        size_t lastSlash = content.find_last_of('/');
        std::string name = content.substr(lastSlash + 1);
        if (!name.empty() && name.back() == '\n') name.pop_back();
        return name;
    }
    return ""; // Detached HEAD 状态没有分支名
}

void RefManager::createBranch(const std::string& branchName) {
    std::string path = getBranchPath(branchName);

    // 检查分支是否已存在
    if (Utils::exists(path)) {
        Utils::exitWithMessage("A branch with that name already exists.");
    }

    // 获取当前 HEAD 指向的 Commit Hash
    std::string currentCommitHash = resolveHead();
    if (currentCommitHash.empty()) {
        Utils::exitWithMessage("No commit to branch from (repo is empty).");
    }

    // 创建 Ref
    // 分支名作为文件名，target 是当前的 Hash，false 表示直接引用
    Ref newBranch(branchName, currentCommitHash, false);

    // 写入 (.gitlite/refs/heads/branchName)
    Utils::writeContents(path, newBranch.serialize());
}

void RefManager::removeBranch(const std::string& branchName) {
    std::string path = getBranchPath(branchName);

    // 检查分支是否存在
    if (!Utils::exists(path)) {
        Utils::exitWithMessage("A branch with that name does not exist.");
    }

    // 如果 HEAD 指向的就是这个分支，则不能删除
    std::string currentBranch = getCurrentBranchName();
    if (currentBranch == branchName) {
        Utils::exitWithMessage("Cannot remove the current branch.");
    }

    // delete
    Utils::restrictedDelete(path);
}

std::vector<std::string> RefManager::getAllBranchNames() const {

    if (!Utils::isDirectory(MASTER_DIR)) {
        return {};
    }


    std::vector<std::string> branchNames = Utils::plainFilenamesIn(MASTER_DIR);

    return branchNames;
}




std::string Ref::serialize() const {
    if (symbolic) {
        return "ref: " + target + "\n";
    }
    return target + "\n"; // 分支文件只包含 Hash + 换行
}
