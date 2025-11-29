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

