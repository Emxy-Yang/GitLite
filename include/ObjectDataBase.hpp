//
// Created by Yang Jibin on 2025/11/27.
//

#ifndef GITLITE_OBJECTDATABASE_HPP
#define GITLITE_OBJECTDATABASE_HPP

#include <string>
#include <memory>
#include "Utils.h"
#include "Objects.hpp"

class GitObject;
class Commit;
class Blob;

class ObjectDatabase {
private:
    // root path
    const std::string BASE_DIR = ".gitlite/objects";

    //path is like objects/ab/(40 bits hash)
    std::string getObjectPath(const std::string& oid) const;

public:
    void initDatabase();

     // write in
     //param obj is commit/blob return obj's OID(hash)。
    std::string writeObject(GitLiteObject &obj);

     // read & deseriaze
     // param OID  return obj
    std::shared_ptr<GitLiteObject> readObject(const std::string& oid);

    std::string findObjectByPrefix(const std::string& prefix);
};


#endif //GITLITE_OBJECTDATABASE_HPP