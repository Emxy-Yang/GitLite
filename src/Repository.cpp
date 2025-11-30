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

std::string  Repository::getGitliteDir() {
    std::string _path = ".gitlite";
    return _path;
}

