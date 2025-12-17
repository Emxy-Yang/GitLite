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
    friend class RemoteObjectDatabase;
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

    std::string readBlobContent(const std::string &blobHash);

    bool hasObject(const std::string &oid) const;

    void copyObjectFromRemote(const std::string &hash, const std::string &remote_gitlite_path);

    void copyToRemote(const std::string &oid, const std::string &remote_gitlite_path) const;
};

class RemoteObjectDatabase {
private:
    std::string remote_root_dir;

    std::string getRemoteObjectPath(const std::string& oid) const;

public:
    explicit RemoteObjectDatabase(const std::string& gitlite_root_dir);

    std::shared_ptr<GitLiteObject> readObject(const std::string& oid) const;

    void copyToLocal(const std::string& oid, ObjectDatabase& localDB);
};


#endif //GITLITE_OBJECTDATABASE_HPP