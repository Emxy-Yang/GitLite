

#include "ObjectDataBase.hpp"
#include "GitliteException.h"
#include <sstream>
#include <iomanip>
#include <iostream>


std::string ObjectDatabase::getObjectPath(const std::string& oid) const {
    if (oid.length() != 40) {
        throw GitliteException("OID must be 40 characters.");
    }
    //  .gitlite/objects/da/39a3...
    std::string subdir = oid.substr(0, 2);
    std::string filename = oid.substr(2);

    return Utils::join(BASE_DIR, subdir, filename);
}

void ObjectDatabase::initDatabase() {
    // create .gitlite/objects
    Utils::createDirectories(BASE_DIR);
}


std::string ObjectDatabase::writeObject(GitLiteObject& obj) {
    // se (Type + Size + \0 + Content)
    std::string serialized_data = obj.serialize();

    // cal oid
    std::string oid = Utils::sha1(serialized_data);
    obj.set_hash(oid);


    std::string path = getObjectPath(oid);

    if (Utils::exists(path)) {
        return oid;
    }

    // write
    try {
        Utils::writeContents(path, serialized_data);
    } catch (const std::invalid_argument& e) {
        throw std::runtime_error("Error writing object to disk: " + std::string(e.what()));
    }

    return oid;
}


std::shared_ptr<GitLiteObject> ObjectDatabase::readObject(const std::string& oid) {
    std::string path = getObjectPath(oid);
    if (!Utils::exists(path)) {
        throw std::runtime_error("Object not found in database: " + oid);
    }

    // read raw data -> copy from localdatabase
    std::string raw_data;
    try {
        raw_data = Utils::readContentsAsString(path);
    } catch (const std::invalid_argument& e) {
        throw std::runtime_error("Error reading object file: " + std::string(e.what()));
    }

    size_t null_byte_pos = raw_data.find('\0');
    if (null_byte_pos == std::string::npos) {
        throw std::runtime_error("Corrupted object format.");
    }

    std::string header = raw_data.substr(0, null_byte_pos);
    std::string content = raw_data.substr(null_byte_pos + 2);

    std::stringstream header_stream(header);
    std::string type_str;
    size_t size_check;
    header_stream >> type_str >> size_check;

    // std::cerr<<size_check<<std::endl;    //debug
    // std::cerr<<content.size()<<std::endl;    //debug
    if (size_check != content.size()) {
        throw std::runtime_error("Corrupted object: size mismatch.");
    }

    if (type_str == "blob") {
        auto blob = std::make_shared<Blob>();
        blob->deserialize(content);
        blob->set_hash(oid);
        return blob;

    } else if (type_str == "commit") {
        auto commit = std::make_shared<Commit>();

        commit->deserialize(content);
        commit->set_hash(oid);

        return commit;

    } else {
        throw std::runtime_error("Unsupported object type: " + type_str);
    }
}


std::string ObjectDatabase::findObjectByPrefix(const std::string& prefix) {
    if (prefix.length() == 40) {
        if (Utils::exists(getObjectPath(prefix))) {
            return prefix;
        }
        return "";
    }

    //cant too short
    if (prefix.length() < 2) {
        return "";
    }

    std::string dirPrefix = prefix.substr(0, 2);
    std::string filePrefix = prefix.substr(2);

    std::string objectDir = Utils::join(".gitlite/objects", dirPrefix);

    if (!Utils::isDirectory(objectDir)) {
        return "";
    }

    std::vector<std::string> files = Utils::plainFilenamesIn(objectDir);
    if (prefix.length() == 2) {
        return dirPrefix + files[0];
    }
    for (const std::string& file : files) {
        if (file.rfind(filePrefix, 0) == 0) {
            return dirPrefix + file;
        }
    }

    return "";
}

std::string ObjectDatabase::readBlobContent(const std::string& blobHash) {
    if (blobHash.empty()) {
        return "";
    }
    std::shared_ptr<GitLiteObject> obj = nullptr;
    try {
        obj = readObject(blobHash);
    } catch (...) {
        return "";
    }
    std::shared_ptr<Blob> blob = std::dynamic_pointer_cast<Blob>(obj);

    if (!blob) {
        return "";
    }
    return blob->getContent();
}

bool ObjectDatabase::hasObject(const std::string& oid) const {
    return Utils::exists(getObjectPath(oid));
}


void ObjectDatabase::copyToRemote(const std::string& oid, const std::string& remote_gitlite_path) const {
    // build local dir
    std::string local_obj_path = getObjectPath(oid); // getObjectPath() 适用于当前的 ObjectDatabase


    // build remote path
    std::string remote_subdir = oid.substr(0, 2);
    std::string remote_filename = oid.substr(2);
    std::string remote_obj_path = Utils::join(remote_gitlite_path, "objects");
    remote_obj_path = Utils::join(remote_obj_path,remote_subdir);
    remote_obj_path = Utils::join(remote_obj_path,remote_filename);

    if (Utils::exists(remote_obj_path)) {
        return;
    }

    // read local content
    std::string content = Utils::readContentsAsString(local_obj_path);

    // write to remote
    std::string remote_dir = Utils::join(remote_gitlite_path, "objects", remote_subdir);
    Utils::createDirectories(remote_dir);

    Utils::writeContents(remote_obj_path, content);
}



RemoteObjectDatabase::RemoteObjectDatabase(const std::string& gitlite_root_dir)
    : remote_root_dir(gitlite_root_dir) {
}

// build obj's remote path
std::string RemoteObjectDatabase::getRemoteObjectPath(const std::string& oid) const {
    if (oid.length() != 40) {
        throw GitliteException("OID must be 40 characters.");
    }
    // path: [remote_root_dir]/objects/da/39a3...
    std::string subdir = oid.substr(0, 2);
    std::string filename = oid.substr(2);

    std::string remotepath = Utils::join(remote_root_dir, "objects");
    remotepath = Utils::join(remotepath,subdir);
    remotepath = Utils::join(remotepath,filename);

    return remotepath;
}

//reuse the local one
std::shared_ptr<GitLiteObject> RemoteObjectDatabase::readObject(const std::string& oid) const {
    std::string path = getRemoteObjectPath(oid);

    if (!Utils::exists(path)) {
        throw GitliteException("Missing object " + oid.substr(0, 7) + " in remote database.");
    }

    std::string serialized_data = Utils::readContentsAsString(path);

    size_t null_pos = serialized_data.find('\0');
    if (null_pos == std::string::npos) {
        throw GitliteException("Corrupt object file: " + oid.substr(0, 7));
    }

    std::string header = serialized_data.substr(0, null_pos);
    size_t space_pos = header.find(' ');
    if (space_pos == std::string::npos) {
        throw GitliteException("Corrupt object header: " + oid.substr(0, 7));
    }

    std::string type_str_remote = header.substr(0, space_pos);
    std::string content = serialized_data.substr(null_pos + 2);

    // deserialize
    if (type_str_remote == "blob") {
        std::shared_ptr<Blob> blob = std::make_shared<Blob>();
        blob->deserialize(content);
        blob->set_hash(oid);
        return blob;
    } else if (type_str_remote == "commit") {
        std::shared_ptr<Commit> commit = std::make_shared<Commit>();
        commit->deserialize(content);
        commit->set_hash(oid);
        return commit;
    } else {
        throw std::runtime_error("Unsupported remote object type: " + type_str_remote);
    }
}

void RemoteObjectDatabase::copyToLocal(const std::string& oid, ObjectDatabase& localDB) {
    if (localDB.hasObject(oid)) {
        return;
    }

    std::string remote_obj_path = getRemoteObjectPath(oid);

    if (!Utils::exists(remote_obj_path)) {
        throw GitliteException("Fatal: Missing object " + oid.substr(0, 7) + " in remote repository.");
    }

    std::string content = Utils::readContentsAsString(remote_obj_path);

    // write(reuse local one)
    std::string local_obj_path = localDB.getObjectPath(oid);

    // path: .gitlite/objects/da
    std::string subdir = oid.substr(0, 2);
    std::string local_subdir = Utils::join(".gitlite", "objects", subdir);
    Utils::createDirectories(local_subdir);

    //write
    Utils::writeContents(local_obj_path, content);

}
