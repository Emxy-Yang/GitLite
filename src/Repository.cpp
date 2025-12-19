#include "../include/Repository.hpp"

#include <queue>
#include <set>

#include "Objects.hpp"
#include "Utils.h"
#include "ObjectDataBase.hpp"
#include"RefManager.hpp"
#include<string>
#include <unordered_set>

#include "RemoteManager.hpp"

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
                if (currentCommit->getMessage() == message) {
                    matching_commits.push_back(currentCommit->get_hashid());
                 }

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
    ObjectDatabase db;
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


    //handle modified and untracked file
    std::string currentCommitHash = ref_manager.resolveHead();
    std::map<std::string, std::string> trackedBlobs;

    if (!currentCommitHash.empty()) {
        auto obj = db.readObject(currentCommitHash);
        std::shared_ptr<Commit> currentCommit = std::dynamic_pointer_cast<Commit>(obj);
        if (currentCommit) {
            trackedBlobs = currentCommit->getBlobs();
        }
    }

    std::set<std::string> allFiles;
    std::vector<std::string> workingFiles = Utils::plainFilenamesIn(".");
    for (const std::string& file : workingFiles) {
        if (file == ".gitlite") continue;
        allFiles.insert(file);
    }
    for (const auto& pair : trackedBlobs) {
        allFiles.insert(pair.first);
    }
    for (const auto& pair : staging_index.getEntries()) {
        allFiles.insert(pair.first);
    }
    for (const std::string& file : staging_index.getRmEntries()) {
        allFiles.insert(file);
    }

    // get file hash - check if modified
    auto getFileHash = [](const std::string& path) -> std::string {
        if (!Utils::exists(path)) return "";
        std::string content = Utils::readContentsAsString(path);
        std::string header = "blob " + std::to_string(content.size() + 1);
        return Utils::sha1(header + '\0'+'\n' + content + '\n');
    };

    std::map<std::string, std::string> modificationsNotStagedMap;
    std::vector<std::string> untrackedFiles;

    for (const std::string& filePath : allFiles) {
        bool inWorkingDir = Utils::exists(filePath);
        bool isTracked = (trackedBlobs.count(filePath) > 0);
        bool inStagedAdd = (staging_index.getEntries().count(filePath) > 0);
        bool inStagedRemove = (std::find(staging_index.getRmEntries().begin(),staging_index.getRmEntries().end(),filePath) != staging_index.getRmEntries().end());


        std::string wdHash = inWorkingDir ? getFileHash(filePath) : "";
        std::string trackedHash = isTracked ? trackedBlobs.at(filePath) : "";
        std::string stagedHash = inStagedAdd ? staging_index.getEntries().at(filePath) : "";

        bool wdContentChangedFromTracked = (isTracked && inWorkingDir && (wdHash != trackedHash));
        bool wdContentChangedFromStaged = (inStagedAdd && inWorkingDir && (wdHash != stagedHash));


        // Untracked Files
        //untracked and unstaged
        if (inWorkingDir && !isTracked && !inStagedAdd) {
            untrackedFiles.push_back(filePath);
            continue;
        }

        //rm-staged but existed in workdir
        if (inStagedRemove && inWorkingDir) {
            untrackedFiles.push_back(filePath);
            continue;
        }


        //Modifications Not Staged For Commit
        //tracked but modified(not staged) (-> modified)
        if (isTracked && wdContentChangedFromTracked && !inStagedAdd && !inStagedRemove) {
            modificationsNotStagedMap[filePath] = "(modified)";
            continue;
        }

        //modified but changed(entry hasn't be upgraded) (-> modified)
        if (inStagedAdd && wdContentChangedFromStaged) {
            modificationsNotStagedMap[filePath] = "(modified)";
            continue;
        }

        //staged but deleted in workdir (-> deleted)
        if (inStagedAdd && !inWorkingDir) {
            modificationsNotStagedMap[filePath] = "(deleted)";
            continue;
        }

        //tracked and not in rm_stage but deleted in workdir (-> deleted)
        if (isTracked && !inStagedRemove && !inWorkingDir) {
            if (!inStagedAdd) {
                modificationsNotStagedMap[filePath] = "(deleted)";
                continue;
            }
        }

    }

    std::cout << "=== Modifications Not Staged For Commit ===" << "\n";
    // std::map 默认按键 (filePath) 排序
    for (const auto& pair : modificationsNotStagedMap) {
        std::cout << pair.first << " " << pair.second << "\n";
    }
    std::cout << "\n";

    std::sort(untrackedFiles.begin(), untrackedFiles.end());
    std::cout << "=== Untracked Files ===" << "\n";
    for (const std::string& file_path : untrackedFiles) {
        std::cout << file_path << "\n";
    }
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

    //write to workdir
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
//rmk: if a branch was deleted but still checkout, the action will succeed and the commit and its father commit will be protected(not be cleared by GC)


void Repository::checkoutBranch(const std::string& branchName) {
    ObjectDatabase db;
    RefManager refManager;
    index idx;

    bool isLocalBranch = false;
    std::string targetCommitHash;

    // local branch: .gitlite/refs/heads/[branchName]
    std::string localBranchPath = Utils::join(".gitlite/refs/heads", branchName);

    // remote branch: .gitlite/refs/remotes/[branchName]
    std::string remoteBranchPath = Utils::join(".gitlite/refs/remotes", branchName);

    if (Utils::exists(localBranchPath)) {
        isLocalBranch = true;
        targetCommitHash = Utils::readContentsAsString(localBranchPath);
    } else if (Utils::exists(remoteBranchPath)) {
        isLocalBranch = false;
        targetCommitHash = Utils::readContentsAsString(remoteBranchPath);
    } else {
        Utils::exitWithMessage("No such branch exists.");
    }

    // find if branch exist
    std::string currentBranch = refManager.getCurrentBranchName();
    if (branchName == currentBranch) {
        Utils::exitWithMessage("No need to checkout the current branch.");
    }

    // get commit hash
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


    //if a file untracked by current_commit and not in idx but in target_blob, then throw an RE and exit

    std::vector<std::string> workingFiles = Utils::plainFilenamesIn(".");
    for (const std::string& file : workingFiles) {
        if (file == ".gitlite") continue;

        bool isTrackedCurrent = (currentBlobs.find(file) != currentBlobs.end());
        bool isStaged = idx.contains_in_entries(file) || idx.contains_in_removed(file);
        bool existsInTarget = (targetBlobs.find(file) != targetBlobs.end());

        if (!isTrackedCurrent && !isStaged && existsInTarget) {
            Utils::exitWithMessage("There is an untracked file in the way; delete it, or add and commit it first.");
        }
    }

    // renew workdir
    //delete
    for (const auto& pair : currentBlobs) {
        const std::string& path = pair.first;
        if (targetBlobs.find(path) == targetBlobs.end()) {
            Utils::restrictedDelete(path);
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

    if (isLocalBranch) {
        // switch branch "ref: refs/heads/[branchName]"
        std::string newHeadContent = "ref: refs/heads/" + branchName + "\n";
        Utils::writeContents(".gitlite/HEAD", newHeadContent);
    } else {
        //detached head
        Utils::writeContents(".gitlite/HEAD", targetCommitHash + "\n");
    }

    idx.clear();
    idx.write();
}

void Repository::branch(const std::string& branchName) {
    RefManager refManager;
    refManager.createBranch(branchName);
}
void Repository::rm_branch(const std::string& branchName) {
    RefManager refManager;
    refManager.removeBranch(branchName);
}

void Repository::reset(const std::string& commitId) {
    ObjectDatabase db;
    RefManager refManager;
    index idx;
    std::string targetHash = db.findObjectByPrefix(commitId);
    if (targetHash.empty()) {
        Utils::exitWithMessage("No commit with that id exists.");
    }

    //load commit
    std::shared_ptr<Commit> targetCommit = nullptr;
    try {
        auto obj = db.readObject(targetHash);
        targetCommit = std::dynamic_pointer_cast<Commit>(obj);
    } catch (...) {
        Utils::exitWithMessage("No commit with that id exists.");
    }
    std::string currentCommitHash = refManager.resolveHead();
    std::shared_ptr<Commit> currentCommit = nullptr;
    if (!currentCommitHash.empty()) {
        try {
            auto obj = db.readObject(currentCommitHash);
            currentCommit = std::dynamic_pointer_cast<Commit>(obj);
        } catch (...) {}
    }

    //load bolb->file
    std::map<std::string, std::string> currentBlobs;
    if (currentCommit) currentBlobs = currentCommit->getBlobs();

    std::map<std::string, std::string> targetBlobs;
    if (targetCommit) targetBlobs = targetCommit->getBlobs();


    //reuse checkout branch code
    std::vector<std::string> workingFiles = Utils::plainFilenamesIn(".");
    for (const std::string& file : workingFiles) {
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
            Utils::restrictedDelete(path);
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


    std::string currentBranch = refManager.getCurrentBranchName();

    if (!currentBranch.empty()) {
        std::string branchPath = Utils::join(".gitlite/refs/heads", currentBranch);
        Utils::writeContents(branchPath, targetHash);
    } else {
        Utils::writeContents(".gitlite/HEAD", targetHash);
    }

    idx.clear();
    idx.write();
}

void Repository::merge(const std::string& givenBranchName) {
    ObjectDatabase db;
    RefManager refManager;
    index idx;

    std::string currentBranchName = refManager.getCurrentBranchName();

    if (currentBranchName == givenBranchName) {
        Utils::exitWithMessage("Cannot merge a branch with itself.");
    }

    std::string givenBranchPath = refManager.getBranchPath(givenBranchName);
    if (!Utils::exists(givenBranchPath)) {
        // to support fetch & pull
        std::string remoteTrackingPath = refManager.getRemoteTrackingBranchPath(givenBranchName);

        if (remoteTrackingPath.empty()) {
            Utils::exitWithMessage("A branch with that name does not exist.");
            return;
        } else {
            givenBranchPath = remoteTrackingPath;
        }
    }

    //clear idx
    if (!idx.getEntries().empty() || !idx.getRmEntries().empty()) {
        Utils::exitWithMessage("You have uncommitted changes.");
    }

    std::string currentHash = refManager.resolveHead();
    std::string givenHash = Utils::readContentsAsString(givenBranchPath);
    if (!givenHash.empty() && givenHash.back() == '\n') givenHash.pop_back();

    // load commit
    std::shared_ptr<Commit> currentCommit = std::dynamic_pointer_cast<Commit>(db.readObject(currentHash));
    std::shared_ptr<Commit> givenCommit = std::dynamic_pointer_cast<Commit>(db.readObject(givenHash));

    // find split obj
    std::string splitPointHash = findCommonAncestor(currentHash, givenHash);


    std::shared_ptr<Commit> splitCommit = std::dynamic_pointer_cast<Commit>(db.readObject(splitPointHash));

    //get all blobs
    std::map<std::string, std::string> splitBlobs = splitCommit->getBlobs();
    std::map<std::string, std::string> currentBlobs = currentCommit->getBlobs();
    std::map<std::string, std::string> givenBlobs = givenCommit->getBlobs();


    std::vector<std::string> workingFiles = Utils::plainFilenamesIn(".");

    //tackle untrack conflict
    for (const std::string& file : workingFiles) {
        if (file == ".gitlite") continue;
        bool isTrackedCurrent = (currentBlobs.count(file));
        bool isStaged = idx.contains_in_entries(file) || idx.contains_in_removed(file);
        bool existsInGiven = (givenBlobs.count(file));

        //if a file untracked by current_commit and not in idx but in given_commit, then throw an RE and exit
        if (!isTrackedCurrent && !isStaged && existsInGiven) {
            Utils::exitWithMessage("There is an untracked file in the way; delete it, or add and commit it first.");
        }
    }

    if (splitPointHash == givenHash) {
        Utils::exitWithMessage("Given branch is an ancestor of the current branch.");
        return;
    }


    /* start merge */

    // Current branch is an ancestor of the given branch (Fast-Forward)
    if (splitPointHash == currentHash) {
        //like checkout to commit
        std::vector<std::string> workingFiles = Utils::plainFilenamesIn(".");
        for (const std::string& file : workingFiles) {
            if (file == ".gitlite") continue;

            bool isTrackedCurrent = (currentBlobs.count(file));
            bool isStaged = idx.contains_in_entries(file) || idx.contains_in_removed(file);
            bool existsInTarget = (givenBlobs.count(file));

            if (!isTrackedCurrent && !isStaged && existsInTarget) {
                Utils::exitWithMessage("There is an untracked file in the way; delete it, or add and commit it first.");
            }
        }


        //update stage and workfir

        // 删除 currentBlobs 中有，但 givenBlobs 中没有的文件
        for (const auto& pair : currentBlobs) {
            const std::string& path = pair.first;
            if (givenBlobs.find(path) == givenBlobs.end()) {
                Utils::restrictedDelete(path);
            }
        }

        // 添加/覆盖 givenBlobs 中的文件，并暂存
        for (const auto& pair : givenBlobs) {
            const std::string& path = pair.first;
            const std::string& blobHash = pair.second;

            writeBlobToWD(db, path, blobHash);

            idx.add_entry(path, blobHash);
        }

        refManager.updateRef("HEAD", givenHash);

        idx.clear();
        idx.write();

        Utils::exitWithMessage("Current branch fast-forwarded.");
        return;
    }

    performThreeWayMerge(splitBlobs, currentBlobs, givenBlobs, idx, db, refManager, givenBranchName, currentBranchName, currentHash, givenHash);
}

void Repository::performThreeWayMerge(
    const std::map<std::string, std::string>& splitBlobs,
    const std::map<std::string, std::string>& currentBlobs,
    const std::map<std::string, std::string>& givenBlobs,
    index& idx,
    ObjectDatabase& db,
    RefManager &refManager,
    const std::string& givenBranchName,
    const std::string& currentBranchName,
    const std::string& currentHash,
    const std::string& givenHash
) {
    std::set<std::string> allFiles;
    for (const auto& pair : splitBlobs) allFiles.insert(pair.first);
    for (const auto& pair : currentBlobs) allFiles.insert(pair.first);
    for (const auto& pair : givenBlobs) allFiles.insert(pair.first);

    bool conflictEncountered = false;

    for (const std::string& path : allFiles) {
        // 获取三个版本的文件哈希 (如果不存在则为空字符串)
        std::string h_split = splitBlobs.count(path) ? splitBlobs.at(path) : "";
        std::string h_current = currentBlobs.count(path) ? currentBlobs.at(path) : "";
        std::string h_given = givenBlobs.count(path) ? givenBlobs.at(path) : "";

        // 定义文件状态 (是否存在)
        bool exists_split = !h_split.empty();
        bool exists_current = !h_current.empty();
        bool exists_given = !h_given.empty();

        // 文件是否被修改 (与 split 相比)
        bool mod_current = exists_current && (h_current != h_split);
        bool mod_given = exists_given && (h_given != h_split);

        // 文件是否被删除 (与 split 相比)
        bool del_current = exists_split && !exists_current;
        bool del_given = exists_split && !exists_given;

        //HANDLE CONFLICT

        // Case 8: modified by different ways
        if (
            // A. 两边都修改，且内容不同
            (mod_current && mod_given && h_current != h_given) ||
            // B. 一边修改，一边删除
            (mod_current && del_given) || (del_current && mod_given) ||
            // C. 均新增，但内容不同
            (!exists_split && exists_current && exists_given && h_current != h_given)
        ) {
            conflictEncountered = true;

            // 获取冲突文件内容
            std::string content_current = h_current.empty() ? "" : db.readBlobContent(h_current);
            std::string content_given = h_given.empty() ? "" : db.readBlobContent(h_given);

            // 构造冲突标记内容
            std::stringstream conflict_ss;
            conflict_ss << "<<<<<<< HEAD\n";
            conflict_ss << content_current;
            if (!content_current.empty() && content_current.back() != '\n') conflict_ss << "\n";
            conflict_ss << "=======\n";
            conflict_ss << content_given;
            if (!content_given.empty() && content_given.back() != '\n') conflict_ss << "\n";
            conflict_ss << ">>>>>>>\n";

            std::string final_content = conflict_ss.str();
            Utils::writeContents(path, final_content);

            // 暂存冲突结果
            Blob conflict_blob(final_content);
            std::string conflict_hash = db.writeObject(conflict_blob);
            idx.add_entry(path, conflict_hash);

            continue;
        }


        //handle no-conflict

        // Case 6: only be deleted in given -> all delete
        if (exists_split && (h_current == h_split) && del_given) {
             Utils::restrictedDelete(path);
             idx.rm_entry(path);
             idx.add_rm_entry(path);
             continue;
        }

        // Case 3: same modify/delete
        if ((mod_current && mod_given && h_current == h_given) || (del_current && del_given)) {
            continue;
        }

        // Case 2: only modified in current -> stay like current
        if (mod_current && !del_given) {
            continue;
        }

        // Case 1: only modified in given -> change to given
        if (mod_given && !del_current) {
            writeBlobToWD(db, path, h_given);
            idx.add_entry(path, h_given);
            continue;
        }

        // Case 5: newly add in given
        if (!exists_split && !exists_current && exists_given) {
            writeBlobToWD(db, path, h_given);
            idx.add_entry(path, h_given);
            continue;
        }

        // Case 4: newly add in current
        if (!exists_split && exists_current && !exists_given) {
            idx.add_entry(path, h_current);
            continue;
        }

    }

    idx.write();


    // COMMIT
    if (conflictEncountered) {
        std::cout << "Encountered a merge conflict." << std::endl;
    }

    std::string mergeMessage = "Merged " + givenBranchName + " into " + currentBranchName + '.';

    Commit newCommit;
    newCommit.setMetadata(mergeMessage, Utils::getCurrentTimestamp());
    newCommit.addFather(currentHash); // HEAD (Current Branch)
    newCommit.addFather(givenHash);   // Given Branch

    newCommit.getBlobsRef() = currentBlobs;
    newCommit.setBlobsFromIndex(idx);

    std::string newCommitHash = db.writeObject(newCommit);

    refManager.updateRef("HEAD", newCommitHash);

    idx.clear();
    idx.write();
}

//a function to act like checkout branch which write blob to working dir
void Repository::writeBlobToWD(ObjectDatabase& db, const std::string& path, const std::string& blobHash) {
    if (blobHash.empty()) {
        return;
    }

    auto obj = db.readObject(blobHash);
    auto blob = std::dynamic_pointer_cast<Blob>(obj);

    Utils::writeContents(path, blob->getContent());
}

std::string Repository::findCommonAncestor(const std::string& hash1, const std::string& hash2) {
    ObjectDatabase db;
    if (hash1 == hash2) {
        return hash1;
    }

    // store hash1 and all its ancestor
    std::unordered_set<std::string> ancestors1;
    std::queue<std::string> queue1;

    queue1.push(hash1);
    ancestors1.insert(hash1);

    //handle multi-father situation (如菱形结构)
    while (!queue1.empty()) {
        std::string currentHash = queue1.front();
        queue1.pop();

        std::shared_ptr<Commit> currentCommit = nullptr;
        auto obj = db.readObject(currentHash);
        currentCommit = std::dynamic_pointer_cast<Commit>(obj);


        if (currentCommit) {
            // 遍历所有父提交 (可能不止一个，处理合并提交)
            for (const std::string& parentHash : currentCommit->getFatherCommits()) {
                if (ancestors1.find(parentHash) == ancestors1.end()) {
                    ancestors1.insert(parentHash);
                    queue1.push(parentHash);
                }
            }
        }
    }


    std::queue<std::string> queue2;
    std::unordered_set<std::string> visited2;

    queue2.push(hash2);
    visited2.insert(hash2);

    while (!queue2.empty()) {
        std::string currentHash = queue2.front();
        queue2.pop();

        if (ancestors1.count(currentHash)) {
            return currentHash;
        }

        std::shared_ptr<Commit> currentCommit = nullptr;
        auto obj = db.readObject(currentHash);
        currentCommit = std::dynamic_pointer_cast<Commit>(obj);

        if (currentCommit) {
            for (const std::string& parentHash : currentCommit->getFatherCommits()) {
                if (visited2.find(parentHash) == visited2.end()) {
                    visited2.insert(parentHash);
                    queue2.push(parentHash);
                }
            }
        }
    }
    return "";
}

void Repository::addRemote(const std::string& name, const std::string& path) {
    RemoteManager remoteManager;
    remoteManager.addRemote(name, path);
}

void Repository::rmRemote(const std::string& name) {
    RemoteManager remoteManager;
    remoteManager.rmRemote(name);
}


//From the local repository's object database, identify all Commit objects and their associated Blob objects that trace upwards from end_hash up to,
//but not including, start_hash, and copy these objects to the specified remote repository path (remote_gitlite_path).
void Repository::traverseAndCopy(
    const std::string& start_hash,
    const std::string& end_hash,
    ObjectDatabase& local_db,
    const std::string& remote_gitlite_path
) {
    std::queue<std::string> q;
    std::unordered_set<std::string> visited;

    q.push(end_hash);
    visited.insert(end_hash);

    while (!q.empty()) {
        std::string current_hash = q.front();
        q.pop();

        if (current_hash == start_hash) {
            continue;
        }

        local_db.copyToRemote(current_hash, remote_gitlite_path);

        std::shared_ptr<Commit> commit = std::dynamic_pointer_cast<Commit>(local_db.readObject(current_hash));
        if (!commit) continue;

        for (const auto& pair : commit->getBlobs()) {
            local_db.copyToRemote(pair.second, remote_gitlite_path);
        }

        for (const std::string& parent_hash : commit->getFatherCommits()) {
            if (visited.find(parent_hash) == visited.end()) {
                visited.insert(parent_hash);
                q.push(parent_hash);
            }
        }
    }
}

void Repository::push(const std::string& remoteName, const std::string& remoteBranchName) {
    RemoteManager remoteManager;
    RefManager localRefManager;

    std::string remote_path = remoteManager.getRemotePath(remoteName);
    if (remote_path.empty() || !Utils::exists(remote_path)) {
        Utils::exitWithMessage("Remote directory not found.");
    }

    RemoteRefManager remoteRefManager(remote_path);
    ObjectDatabase localDB;

    std::string local_hash = localRefManager.resolveHead();
    std::string remote_ref_name = "refs/heads/" + remoteBranchName;
    std::string remote_hash = remoteRefManager.resolveRef(remote_ref_name);

    if (remote_hash.empty()) {
        //新建分支 (首次推送)
        traverseAndCopy("", local_hash, localDB, remote_path);

        // 在远程创建新分支并指向本地 HEAD
        remoteRefManager.updateRef(remote_ref_name, local_hash);
        Utils::exitWithMessage("New remote branch created and pushed.");
        return;
    }

    //Fast-Forward 检查：检查远程 HEAD 是否是本地 HEAD 的祖先
    try {
        localDB.readObject(remote_hash);
    } catch (...) {
        Utils::exitWithMessage("Please pull down remote changes before pushing.");
        return;
    }  //debug
    auto ancestor= findCommonAncestor(remote_hash,local_hash);
    if (ancestor != remote_hash)
    {
        Utils::exitWithMessage("Please pull down remote changes before pushing.");
        return;
    }

    // III. 执行 Push (Fast-Forward)

    // 复制对象：只复制 remote_hash 之后的 Commit 和 Blob
    traverseAndCopy(remote_hash, local_hash, localDB, remote_path);

    // 更新远程引用
    remoteRefManager.updateRef(remote_ref_name, local_hash);
}

void Repository::fetch(const std::string& remoteName, const std::string& remoteBranchName) {
    RemoteManager remoteManager;

    std::string remote_path = remoteManager.getRemotePath(remoteName);
    if (remote_path.empty() || !Utils::exists(remote_path)) {
        Utils::exitWithMessage("Remote directory not found.");
    }

    RemoteObjectDatabase remoteDB(remote_path);
    RemoteRefManager remoteRefManager(remote_path);

    std::string remote_ref_name = "refs/heads/" + remoteBranchName;
    std::string remote_hash = remoteRefManager.resolveRef(remote_ref_name);

    if (remote_hash.empty()) {
        Utils::exitWithMessage("That remote does not have that branch.");
    }

    //复制对象 (从远程到本地)
    std::queue<std::string> q;
    std::unordered_set<std::string> visited;

    q.push(remote_hash);
    visited.insert(remote_hash);

    ObjectDatabase localDB; // 本地数据库

    while (!q.empty()) {
        std::string current_hash = q.front();
        q.pop();

        remoteDB.copyToLocal(current_hash, localDB);

        //读取对象以获取其关联的 Blob 和父级 Commit
        std::shared_ptr<Commit> commit = std::dynamic_pointer_cast<Commit>(remoteDB.readObject(current_hash));
        if (!commit) continue;

        //复制 Commit 关联的 Blob 对象
        for (const auto& pair : commit->getBlobs()) {
            remoteDB.copyToLocal(pair.second, localDB);
        }

        //遍历父级 Commit
        for (const std::string& parent_hash : commit->getFatherCommits()) {
            if (visited.find(parent_hash) == visited.end()) {
                visited.insert(parent_hash);
                q.push(parent_hash);
            }
        }
    }

    // III. 更新本地跟踪引用
    RefManager localRefManager;
    std::string local_tracking_ref = "refs/remotes/" + remoteName + "/" + remoteBranchName;

    localRefManager.updateRemoteRef(remoteName, remoteBranchName, remote_hash);
}

void Repository::pull(const std::string& remoteName, const std::string& remoteBranchName) {
    this->fetch(remoteName, remoteBranchName);
    std::string trackingBranchName = remoteName + "/" + remoteBranchName;
    this->merge(trackingBranchName);
}



std::string  Repository::getGitliteDir() {
    std::string _path = ".gitlite";
    return _path;
}

