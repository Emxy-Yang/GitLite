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

    if (Utils::exists(".gitlite")) {
    std::cout << "Initialized empty Gitlite repository." << std::endl;
    }
}

void Repository::add(std::string & file) {
    if (!Utils::isFile(file)) {
        Utils::exitWithMessage("File Doesn't Exist");
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

        if (idx.contains(file)) {
            idx.rm_entry(file);
            idx.write();
        }
        return;
    }

    db.writeObject(add_blob);
    idx.add_entry(file, new_blob_hash);
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
    if (idx.getEntries().empty()) {
        Utils::exitWithMessage("No changes added to commit.");
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
    bool isStagedForAddition = idx.contains(file_name);


    if (!isStagedForAddition && !isTrackedByCommit) {
        Utils::exitWithMessage("No reason to remove the file.");
        return;
    }

    // Untracked and Staged
    if (isStagedForAddition && !isTrackedByCommit) {
        // remove from entries
        idx.rm_entry(file_name);
        Utils::message("Unstaged " + file_name);
    }
    //Tracked by Current Commit
    else if (isTrackedByCommit) {
        idx.rm_entry(file_name);

        //remove from working dialoge
        if (Utils::exists(file_name)) {
            if (Utils::restrictedDelete(file_name)) {
                 Utils::message("Removed " + file_name + " from working directory and staged for deletion.");
            } else {
                 Utils::message("Staged " + file_name + " for deletion. Note: Failed to remove file from working directory.");
            }
        } else {
             Utils::message("Staged " + file_name + " for deletion (file already missing).");
        }
    }

    //refresh index
    idx.write();
}

void Repository::log() {
    ObjectDatabase db;
    RefManager refManager;

    std::string currentCommitHash = refManager.resolveHead();
    if (currentCommitHash.empty()) {
        Utils::exitWithMessage("No commits yet in this repository.");
    }

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

    for (const std::string& subdir : subdirs) {
        std::string subdir_path = Utils::join(OBJECTS_DIR, subdir);

        std::vector<std::string> object_files = Utils::plainFilenamesIn(subdir_path);

        for (const std::string& filename : object_files) {
            // 重构完整的 Commit ID (OID)，例如：da39a3...
            std::string commit_hash = subdir + filename;

            std::shared_ptr<GitLiteObject> obj = nullptr;

            try {
                obj = db.readObject(commit_hash);
            } catch (const std::exception& e) {
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

std::string  Repository::getGitliteDir() {
    std::string _path = ".gitlite";
    return _path;
}

