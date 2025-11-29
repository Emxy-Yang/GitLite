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
        // 假设路径总是标准的 refs/heads/
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

    // 1检查分支是否存在
    if (!Utils::exists(path)) {
        Utils::exitWithMessage("A branch with that name does not exist.");
    }

    // 防止删除当前所在的分支
    // 如果 HEAD 指向的就是这个分支，则不能删除
    std::string currentBranch = getCurrentBranchName();
    if (currentBranch == branchName) {
        Utils::exitWithMessage("Cannot remove the current branch.");
    }

    // delete
    Utils::restrictedDelete(path);
}






std::string Ref::serialize() const {
    if (symbolic) {
        return "ref: " + target + "\n";
    }
    return target + "\n"; // 分支文件只包含 Hash + 换行
}
