#include "index.hpp"
#include "Utils.h"
#include "GitliteException.h"
#include "Objects.hpp"
#include <sstream>
#include <utility>

void index::add_entry(std::string path , std::string hash) {
    entries[path] = std::move(hash);
}

void index::add_rm_entry(std::string path) {
    removed_entries.emplace_back(path);
}

void index::rm_entry(const std::string& path) {
    entries.erase(path);
}

void index::rm_rmentry(const std::string &file) {
    removed_entries.erase(std::find(removed_entries.begin(),removed_entries.end(),file));
}

bool index::contains_in_entries(const std::string& path) {
    return entries.find(path) != entries.end();
}

bool index::indentical(const std::string &path, Blob & obj) {
    return entries[path] == obj.get_hashid();
}


void index::write() {
    //delete old index
    if (Utils::exists(INDEX_PATH)) {
        std::remove(INDEX_PATH.c_str());
    }

    //create new one
    std::stringstream ss;
    for (const auto& pair : entries) {
        // hash + \space  + path + '\n'
        ss << pair.second << " " << pair.first << "\n";
    }

    const std::string REMOVED_DELIMITER = "----RMD----\n";
    ss<<REMOVED_DELIMITER;
    for (const auto& path : removed_entries) {
        ss << path << "\n";
    }

    Utils::writeContents(INDEX_PATH, ss.str());
}

void index::load() {
    if (!Utils::exists(INDEX_PATH)) {
        entries.clear();
    }
    else {
        entries.clear();
        std::string raw_data;
        try {
            raw_data = Utils::readContentsAsString(INDEX_PATH);
        } catch (const std::invalid_argument& e) {
            throw std::runtime_error("Error reading object file: " + std::string(e.what()));
        }

        //read
        std::stringstream data(raw_data);
        std::string hash;
        std::string path;
        while (data>>hash) {
            const std::string REMOVED_DELIMITER = "----RMD----";
            if (hash == REMOVED_DELIMITER) {
                break;
            }
            if (hash.length() != 40) {
                throw GitliteException("a index row must be hash + path");
            }
            data >> path;
            entries[path] = hash;
            hash.clear() , path.clear();
        }
        while (data>>path) {
            removed_entries.emplace_back(path);
            hash.clear() , path.clear();
        }
    }
}

void index::clear() {
    entries.clear();
    removed_entries.clear();
}



