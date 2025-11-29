

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

    // read raw data
    std::string raw_data;
    try {
        raw_data = Utils::readContentsAsString(path);
    } catch (const std::invalid_argument& e) {
        throw std::runtime_error("Error reading object file: " + std::string(e.what()));
    }

    // 确定类型
    size_t null_byte_pos = raw_data.find('\0');
    if (null_byte_pos == std::string::npos) {
        throw std::runtime_error("Corrupted object format.");
    }

    std::string header = raw_data.substr(0, null_byte_pos);
    std::string content = raw_data.substr(null_byte_pos + 1);

    // 解析 Type 和 Size
    std::stringstream header_stream(header);
    std::string type_str;
    size_t size_check;
    header_stream >> type_str >> size_check;

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