
#ifndef GITLITE_INDEX_HPP
#define GITLITE_INDEX_HPP
#include <algorithm>
#include <map>
#include <string>
#include<memory>
#include"Objects.hpp"

class Blob;
class index {
    std::map<std::string, std::string> entries;     //file path & blob hashid
    std::vector<std::string> removed_entries;  //removed files (path only)
    const std::string INDEX_PATH = ".gitlite/index";

public:
    index(){ load();}

    void add_entry(std::string path , std::string hash);
    void add_rm_entry(std::string path);

    void rm_entry(const std::string& path);
    void rm_rmentry(const std::string & file);


    bool contains_in_entries(const std::string& path);
    bool indentical(const std::string& path , Blob&);

    const std::map<std::string, std::string>& getEntries() const {
        return entries;
    }
    const std::vector<std::string>& getRmEntries() const {
        return removed_entries;
    }


    bool contains_in_removed(const std::string& path) const {
        return find(removed_entries.begin(),removed_entries.end(),path) != removed_entries.end();
    }


    void write();
    void load();
    void clear();

};

#endif //GITLITE_INDEX_HPP