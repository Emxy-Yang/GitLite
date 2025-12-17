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

void RefManager::updateRemoteRef(const std::string& remoteName, const std::string& remoteBranchName, const std::string& newHash) {
    if (newHash.empty() || newHash.length() != 40) {
        Utils::exitWithMessage("Invalid hash provided for remote reference update.");
    }

    // build path: .gitlite/refs/remotes/[remoteName]
    std::string remoteRefDir = Utils::join(".gitlite", "refs");
    remoteRefDir = Utils::join(remoteRefDir, "remotes");
    remoteRefDir = Utils::join(remoteRefDir, remoteName);

    if (!Utils::exists(remoteRefDir)) {
        Utils::createDirectories(remoteRefDir);
    }

    // .gitlite/refs/remotes/[remoteName]/[remoteBranchName]
    std::string fullPath = Utils::join(remoteRefDir, remoteBranchName);

    Utils::writeContents(fullPath, newHash + "\n");
}


std::string RefManager::getRemoteTrackingBranchPath(const std::string& remoteTrackingName) const {
    size_t slashPos = remoteTrackingName.find('/');

    //  remotename/branchname
    if (slashPos == std::string::npos || slashPos == 0 || slashPos == remoteTrackingName.length() - 1) {
        return "";
    }

    std::string remoteName = remoteTrackingName.substr(0, slashPos);
    std::string branchName = remoteTrackingName.substr(slashPos + 1);


    std::string fullPath = Utils::join(".gitlite", "refs");
    fullPath = Utils::join(fullPath,"remotes");
    fullPath = Utils::join(fullPath, remoteName);
    fullPath = Utils::join(fullPath, branchName);

    if (Utils::exists(fullPath)) {
        return fullPath;
    }

    return "";
}

//ref: refs/heads/master
std::string RefManager::resolveHead() {
    std::string content = Utils::readContentsAsString(HEAD_FILE);

    if (!content.empty() && content.back() == '\n') content.pop_back();

    if (content.substr(0, 5) == "ref: ") {
        std::string refPath = ".gitlite/" + content.substr(5);   //dir is like ".gitlite/refs/heads/[branch name]"
        if (!Utils::exists(refPath)) return "";
        std::string hash = Utils::readContentsAsString(refPath);

        if (!hash.empty() && hash.back() == '\n') hash.pop_back();  //debug
        return hash;
    } else {
        // Detached HEAD, return hash
        return content;
    }
}

//ref: refs/heads/master
std::string RefManager::getCurrentBranchName() {
    std::string content = Utils::readContentsAsString(HEAD_FILE);
    if (content.substr(0, 5) == "ref: ") {
        size_t lastSlash = content.find_last_of('/');
        std::string name = content.substr(lastSlash + 1);
        if (!name.empty() && name.back() == '\n') name.pop_back();
        return name;
    }
    return ""; // Detached HEAD has no branch name
}

void RefManager::createBranch(const std::string& branchName) {
    std::string path = getBranchPath(branchName);

    if (Utils::exists(path)) {
        Utils::exitWithMessage("A branch with that name already exists.");
    }

    std::string currentCommitHash = resolveHead();
    if (currentCommitHash.empty()) {
        Utils::exitWithMessage("No commit to branch from (repo is empty).");
    }

    // create  Reference
    Ref newBranch(branchName, currentCommitHash, false);

    // write to .gitlite/refs/heads/[branchName]
    Utils::writeContents(path, newBranch.serialize());
}

void RefManager::removeBranch(const std::string& branchName) {
    std::string path = getBranchPath(branchName);

    if (!Utils::exists(path)) {
        Utils::exitWithMessage("A branch with that name does not exist.");
    }

    //if HEAD point to the branch, then the branch cant br removed
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



RemoteRefManager::RemoteRefManager(const std::string& gitlite_root_dir)
    : remote_root_dir(gitlite_root_dir) {
    if (!remote_root_dir.empty() && remote_root_dir.back() == '/') {
        remote_root_dir.pop_back();
    }
}

std::string RemoteRefManager::getRefPath(const std::string& refName) const {
    // [remote_root_dir]/refs/heads/master
    return Utils::join(remote_root_dir, refName);
}

std::string RemoteRefManager::resolveRef(const std::string& refName) const {
    std::string refPath = getRefPath(refName);

    if (!Utils::exists(refPath)) {
        return "";
    }

    std::string content = Utils::readContentsAsString(refPath);

    if (!content.empty() && content.back() == '\n') {
        content.pop_back();
    }                                                                                                                 //debug
    content.erase(std::remove_if(content.begin(), content.end(), ::isspace), content.end());   //debug

    if (content.length() == 40) {
        return content;
    }
    return "";
}

//remote ref only have detached ref
void RemoteRefManager::updateRef(const std::string& refName, const std::string& newHash) {
    if (newHash.empty() || newHash.length() != 40) {
        Utils::exitWithMessage("Invalid hash provided for reference update.");
    }

    std::string refPath = getRefPath(refName);

    Utils::writeContents(refPath, newHash + "\n");
}



std::string Ref::serialize() const {
    if (symbolic) {
        return "ref: " + target + "\n";
    }
    return target + "\n"; // 分支文件只包含 Hash + 换行
}
