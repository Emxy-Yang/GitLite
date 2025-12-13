#include "../include/Repository.hpp"
#include "Objects.hpp"
#include "Utils.h"
#include "ObjectDataBase.hpp"
#include"RefManager.hpp"
#include<string>

void Repository::init() {
    //check if .gitlite exists
    if (Utils::exists(".gitlite")) {
        Utils::exitWithMessage("A Gitlite version-control system already exists in the current directory.");
    }

    //creat init commit and write
    ObjectDatabase init_obj;
    init_obj.initDatabase();
    Commit init_commit;
    init_obj.writeObject(init_commit);

    //init ref(HEAD file and /ref/heads/path)
    RefManager init_ref;
    init_ref.initManager(init_commit);

}

void Repository::add(std::string & file) {
    if (!Utils::isFile(file)) {
        Utils::exitWithMessage("File does not exist.");
    }

    //read fileContent
    std::string fileContent;
    try {
        fileContent = Utils::readContentsAsString(file);
    } catch (...) {
        Utils::exitWithMessage("Error reading file: " + file);
    }

    //init Bolb
    Blob add_blob(fileContent);

    //init database
    ObjectDatabase db;
    std::string new_blob_hash = Utils::sha1(add_blob.serialize());

    //refresh index and write index
    index idx;
    RefManager refManager;
    std::string head_commit_hash = refManager.resolveHead();

    std::string commit_blob_hash;

    if (!head_commit_hash.empty()) {
        auto commit_obj = db.readObject(head_commit_hash);
        if (commit_obj) {
            // convert gitobj to commit
            auto head_commit =std::dynamic_pointer_cast<Commit>(commit_obj);

            // find blob hash in current commit

            if (head_commit) {
                commit_blob_hash = head_commit->getBlobHash(file);
            }
        }
    }

    //if identical
    if (new_blob_hash == commit_blob_hash) {
        if (idx.contains_in_entries(file)) {
            idx.rm_entry(file);
        }
        if (idx.contains_in_removed(file)) {
            idx.rm_rmentry(file);
        }
        idx.write();
        return;
    }

    db.writeObject(add_blob);
    if (!idx.contains_in_removed(file))
        idx.add_entry(file, new_blob_hash);
    else
        idx.rm_rmentry(file);
    idx.write();
}

void Repository::commit(std::string & message) {
    if (message.empty()) {
        Utils::exitWithMessage("Please enter a commit message.");
    }
    //init
    index idx;
    ObjectDatabase db;
    RefManager refManager;

    //check entries
    if (idx.getEntries().empty() && idx.getRmEntries().empty()) {
        Utils::exitWithMessage("No changes added to the commit.");
    }

    Commit newCommit;
    //set metadata
    std::string timestamp = Utils::getCurrentTimestamp();
    newCommit.setMetadata(message, timestamp);
    //set father commit
    std::string parentHash = refManager.resolveHead();
    if (!parentHash.empty()) {
        newCommit.addFather(parentHash);

        //firsly read and copy father commit
        auto father_commit = std::dynamic_pointer_cast<Commit>(db.readObject(parentHash));
        if (!father_commit) {
            Utils::exitWithMessage("Fail to read father commit!");
        }
        for (const auto& father_blobs : father_commit->getBlobs()) {
            newCommit.addBlob(father_blobs.first , father_blobs.second);
        }
    }

    //then change blobs according to the entries (staging area)
    for (const auto& changed_blobs : idx.getEntries()) {
        newCommit.addBlob(changed_blobs.first , changed_blobs.second);
    }
    for (const auto & changed_blobs : idx.getRmEntries()) {
        if (newCommit.getBlobs().find(changed_blobs) != newCommit.getBlobs().end()) {
            newCommit.rmBlob(changed_blobs);
        }else {
            Utils::exitWithMessage("wrongly deleted inexisted file");
        }
    }

    //then write commit and update refs
    db.writeObject(newCommit);
    refManager.updateRef("HEAD",newCommit.get_hashid());

    //refresh index
    idx.clear();
    idx.write();
}

void Repository::rm(std::string & file_name) {
    index idx;
    ObjectDatabase db;
    RefManager refManager;

    // get current commit state
    std::string currentCommitHash = refManager.resolveHead();
    std::shared_ptr<Commit> currentCommit = nullptr;
    bool isTrackedByCommit = false;

    if (!currentCommitHash.empty()) {
        //judge if the file being tracked by current commit
        try {
            auto obj = db.readObject(currentCommitHash);
            currentCommit = std::dynamic_pointer_cast<Commit>(obj);
            if (currentCommit) {
                //std::cerr<<"exists"<<std::endl;   //debug
                isTrackedByCommit = currentCommit->isTracking(file_name);
            }
        } catch (...) {
            isTrackedByCommit = false;
        }
    }

    // judge if the file is in entries
    bool isStagedForAddition = idx.contains_in_entries(file_name);


    if (!isStagedForAddition && !isTrackedByCommit) {
        Utils::exitWithMessage("No reason to remove the file.");
        return;
    }

    // Untracked and Staged
    if (isStagedForAddition && !isTrackedByCommit) {
        // remove from entries
        idx.rm_entry(file_name);
        //Utils::message("Unstaged " + file_name);
    }
    //Tracked by Current Commit
    else if (isTrackedByCommit) {
        idx.rm_entry(file_name);
        idx.add_rm_entry(file_name);

        //remove from working dialoge
        if (Utils::exists(file_name)) {
            Utils::restrictedDelete(file_name);
        }
    }

    //refresh index
    idx.write();
}

void Repository::log() {
    ObjectDatabase db;
    RefManager refManager;

    std::string currentCommitHash = refManager.resolveHead();

    while (!currentCommitHash.empty()) {
        std::shared_ptr<Commit> currentCommit;
        try {
            auto obj = db.readObject(currentCommitHash);
            currentCommit = std::dynamic_pointer_cast<Commit>(obj);

            if (!currentCommit) {
                Utils::exitWithMessage("Corrupted object: object at " + currentCommitHash.substr(0, 7) + " is not a Commit.");
            }
        } catch (const std::exception& e) {
            Utils::exitWithMessage("Error reading commit object (" + currentCommitHash.substr(0, 7) + "): " + e.what());
        }

        //output commit hashid
        std::cout << "===\ncommit " << currentCommit->get_hashid() << "\n";

        const auto& fathers = currentCommit->getFatherCommits();

        //handle merge
        if (fathers.size() == 2) {
            std::cout << "Merge: " << fathers[0].substr(0, 7) << " "
                      << fathers[1].substr(0, 7) << "\n";
        }

        //output metadata
        std::cout << "Date: " << currentCommit->getTimestamp() << "\n";
        std::cout << currentCommit->getMessage() << "\n";
        std::cout << "\n";

        //search back
        if (!fathers.empty()) {
            currentCommitHash = fathers[0]; //main branch in [0]
        } else {
            currentCommitHash.clear();
        }
    }
}

void Repository::globalLog() {
    ObjectDatabase db;

    const std::string OBJECTS_DIR = ".gitlite/objects";
    std::vector<std::string> subdirs = Utils::plainFilenamesIn(OBJECTS_DIR);

    for (const std::string& subdir : subdirs) {
        //.gitlite/objects/da
        std::string subdir_path = Utils::join(OBJECTS_DIR, subdir);
        std::vector<std::string> object_files = Utils::plainFilenamesIn(subdir_path);

        for (const std::string& filename : object_files) {
            std::string commit_hash = subdir + filename;
            std::shared_ptr<GitLiteObject> obj = nullptr;

            //read from hash
            try {
                obj = db.readObject(commit_hash);
            } catch (const std::exception& e) {
                continue;
            }

            //check if it's commit
            std::shared_ptr<Commit> currentCommit = std::dynamic_pointer_cast<Commit>(obj);
            if (currentCommit) {
                //copy from log
                std::cout << "===\ncommit " << currentCommit->get_hashid() << "\n";
                const auto& fathers = currentCommit->getFatherCommits();

                if (fathers.size() == 2) {
                    std::cout << "Merge: " << fathers[0].substr(0, 7) << " "
                              << fathers[1].substr(0, 7) << "\n";
                }

                std::cout << "Date: " << currentCommit->getTimestamp() << "\n";
                std::cout << currentCommit->getMessage() << "\n";
                std::cout << "\n";
            }
        }
    }
}

void Repository::find(const std::string &message) {
    ObjectDatabase db;
    std::vector<std::string> matching_commits;

    const std::string OBJECTS_DIR = ".gitlite/objects";
    if (!Utils::isDirectory(OBJECTS_DIR)) {
        Utils::exitWithMessage("No Gitlite repository found or objects directory is missing.");
    }

    std::vector<std::string> subdirs = Utils::plainFilenamesIn(OBJECTS_DIR);
    //std::cerr<<subdirs.size()<<std::endl;  //debug
    for (const std::string& subdir : subdirs) {
        std::string subdir_path = Utils::join(OBJECTS_DIR, subdir);

        std::vector<std::string> object_files = Utils::plainFilenamesIn(subdir_path);
        //std::cerr<<object_files.size()<<std::endl;  //debug

        for (const std::string& filename : object_files) {
            // place together 2 bit hash(subdir) and 38 bit
            std::string commit_hash = subdir + filename;

            std::shared_ptr<GitLiteObject> obj = nullptr;

            try {
                obj = db.readObject(commit_hash);
            } catch (const std::exception& e) {
                //std::cerr<<"cant find obj : "<<commit_hash<<std::endl; //debug
                continue;
            }

            std::shared_ptr<Commit> currentCommit = std::dynamic_pointer_cast<Commit>(obj);
            if (currentCommit) {
                // if (!currentCommit->getMessage().empty() && currentCommit->getMessage().back() == '\n') {
                //     currentCommit->getMessage().pop_back();
                // }
                if (currentCommit->getMessage() == message) {
                    matching_commits.push_back(currentCommit->get_hashid());
                 }
                //else {
                //     std::cout<<"mismatched message : "<<currentCommit->getMessage()<<std::endl;
                // }//debug
            }
        }
    }



    if (matching_commits.empty()) {
        Utils::exitWithMessage("Found no commit with that message.");
    } else {
        for (const std::string& hash : matching_commits) {
            std::cout << hash << "\n";
        }
    }
}

void Repository::status() {
    RefManager ref_manager;
    index staging_index;

    std::cout << "=== Branches ===" << "\n";

    std::vector<std::string> all_branches = ref_manager.getAllBranchNames();

    std::string current_branch_name = ref_manager.getCurrentBranchName();

    for (const std::string& branch : all_branches) {
        if (branch == current_branch_name) {
            std::cout << "*" << branch << "\n";
        } else {
            std::cout << branch << "\n";
        }
    }

    std::cout << "\n";


    std::cout << "=== Staged Files ===" << "\n";

    const auto& staged_entries = staging_index.getEntries();

    std::vector<std::string> staged_files;
    for (const auto& pair : staged_entries) {
        staged_files.push_back(pair.first);
    }

    std::sort(staged_files.begin(), staged_files.end());

    for (const std::string& file_path : staged_files) {
        std::cout << file_path << "\n";
    }

    std::cout << "\n";


    std::cout << "=== Removed Files ===" << "\n";

    const auto& removed_files = staging_index.getRmEntries();

    std::vector<std::string> sorted_removed_files = removed_files;
    std::sort(sorted_removed_files.begin(), sorted_removed_files.end());

    for (const std::string& file_path : sorted_removed_files) {
        std::cout << file_path << "\n";
    }

    std::cout << "\n";

    std::cout << "=== Modifications Not Staged For Commit ===" << "\n";
    std::cout << "\n";

    std::cout << "=== Untracked Files ===" << "\n";
    std::cout << "\n";
}

// Repository.cpp

void Repository::checkoutFile(const std::string& fileName) {
    ObjectDatabase db;
    RefManager refManager;
    std::string headCommitHash = refManager.resolveHead();

    if (headCommitHash.empty()) {
        Utils::exitWithMessage("No commit with that id exists.");
    }

    //read head commit
    std::shared_ptr<Commit> headCommit = nullptr;
    try {
        auto obj = db.readObject(headCommitHash);
        headCommit = std::dynamic_pointer_cast<Commit>(obj);
    } catch (...) {
        Utils::exitWithMessage("No commit with that id exists.");
    }

    std::string blobHash = headCommit->getBlobHash(fileName);

    if (blobHash.empty()) {
        Utils::exitWithMessage("File does not exist in that commit.");
    }

    //load blob
    auto blobObj = db.readObject(blobHash);
    auto blob = std::dynamic_pointer_cast<Blob>(blobObj);

    //write to workmenu
    if (blob) {
        Utils::writeContents(fileName, blob->getContent());
    } else {
        Utils::exitWithMessage("File does not exist in that commit.");
    }

}

// Repository.cpp

void Repository::checkoutFileInCommit(const std::string& commitId, const std::string& fileName) {
    ObjectDatabase db;

    std::string targetHash = db.findObjectByPrefix(commitId);

    if (targetHash.empty()) {
        Utils::exitWithMessage("No commit with that id exists.");
    }

    // read target commit
    std::shared_ptr<Commit> targetCommit = nullptr;
    try {
        auto obj = db.readObject(targetHash);
        targetCommit = std::dynamic_pointer_cast<Commit>(obj);
    } catch (...) {
        Utils::exitWithMessage("No commit with that id exists.");
    }

    if (!targetCommit) {
        Utils::exitWithMessage("No commit with that id exists.");
    }

    //find if files in commit
    std::string blobHash = targetCommit->getBlobHash(fileName);

    if (blobHash.empty()) {
        Utils::exitWithMessage("File does not exist in that commit.");
    }

    // read blob
    auto blobObj = db.readObject(blobHash);
    auto blob = std::dynamic_pointer_cast<Blob>(blobObj);

    if (blob) {
        Utils::writeContents(fileName, blob->getContent());
    } else {
        Utils::exitWithMessage("File does not exist in that commit.");
    }
}

// Repository.cpp

void Repository::checkoutBranch(const std::string& branchName) {
    ObjectDatabase db;
    RefManager refManager;
    index idx;

    // find if branch exist
    std::string branchPath = Utils::join(".gitlite/refs/heads", branchName);
    if (!Utils::exists(branchPath)) {
        Utils::exitWithMessage("No such branch exists.");
    }
    std::string currentBranch = refManager.getCurrentBranchName();
    if (branchName == currentBranch) {
        Utils::exitWithMessage("No need to checkout the current branch.");
    }

    // get commit hash
    std::string targetCommitHash = Utils::readContentsAsString(branchPath);
    if (!targetCommitHash.empty() && targetCommitHash.back() == '\n') {
        targetCommitHash.pop_back();
    }//debug
    std::string currentCommitHash = refManager.resolveHead();

    std::shared_ptr<Commit> currentCommit = nullptr;
    std::shared_ptr<Commit> targetCommit = nullptr;

    // load commit
    auto obj = db.readObject(targetCommitHash);
    targetCommit = std::dynamic_pointer_cast<Commit>(obj);

    if (!currentCommitHash.empty()) {
        try {
            auto obj = db.readObject(currentCommitHash);
            currentCommit = std::dynamic_pointer_cast<Commit>(obj);
        } catch (...) {
        }
    }

    // get Blobs Map (Path -> Hash)
    std::map<std::string, std::string> currentBlobs;
    if (currentCommit) currentBlobs = currentCommit->getBlobs();

    std::map<std::string, std::string> targetBlobs;
    if (targetCommit) targetBlobs = targetCommit->getBlobs();




    // Untracked File Check)
    // 遍历工作目录中的所有文件
    // 如果一个文件：
    //   1. 未被当前 Commit 跟踪 (not in currentBlobs)
    //   2. 未被暂存 (not in idx)
    //   3. 但是存在于目标 Commit 中 (in targetBlobs)
    // 那么报错退出。

    std::vector<std::string> workingFiles = Utils::plainFilenamesIn(".");
    for (const std::string& file : workingFiles) {
        // 忽略 .gitlite 目录
        if (file == ".gitlite") continue;

        bool isTrackedCurrent = (currentBlobs.find(file) != currentBlobs.end());
        bool isStaged = idx.contains_in_entries(file) || idx.contains_in_removed(file);
        bool existsInTarget = (targetBlobs.find(file) != targetBlobs.end());

        if (!isTrackedCurrent && !isStaged && existsInTarget) {
            Utils::exitWithMessage("There is an untracked file in the way; delete it, or add and commit it first.");
        }
    }





    // renew workmenu
    //delete
    for (const auto& pair : currentBlobs) {
        const std::string& path = pair.first;
        if (targetBlobs.find(path) == targetBlobs.end()) {
            Utils::restrictedDelete(path); // 删除文件
        }
    }
    //add
    for (const auto& pair : targetBlobs) {
        const std::string& path = pair.first;
        const std::string& blobHash = pair.second;

        try {
            auto obj = db.readObject(blobHash);
            auto blob = std::dynamic_pointer_cast<Blob>(obj);
            if (blob) {
                Utils::writeContents(path, blob->getContent());
            }
        } catch (...) {
            Utils::exitWithMessage("Fatal: Missing blob object for " + path);
        }
    }

    // switch branch "ref: refs/heads/[branchName]"
    std::string newHeadContent = "ref: refs/heads/" + branchName + "\n";
    Utils::writeContents(".gitlite/HEAD", newHeadContent);

    idx.clear();
    idx.write();
}

std::string  Repository::getGitliteDir() {
    std::string _path = ".gitlite";
    return _path;
}

