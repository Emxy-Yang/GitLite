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
    ObjectDatabase add_obj;
    add_obj.writeObject(add_blob);

    //refresh index and write index
    index add_idx;
    if (!add_idx.contains(file) || !add_idx.indentical(file,add_blob)) {
        add_idx.add_entry(file,add_blob.get_hashid());

        add_idx.write();
    }
}

std::string  Repository::getGitliteDir() {
    std::string _path = ".gitlite";
    return _path;
}

