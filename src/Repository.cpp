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

std::string  Repository::getGitliteDir() {
    std::string _path = ".gitlite";
    return _path;
}

