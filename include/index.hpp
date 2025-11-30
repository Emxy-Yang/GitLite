
#ifndef GITLITE_INDEX_HPP
#define GITLITE_INDEX_HPP
#include <map>
#include <string>
#include<memory>
#include"Objects.hpp"

class index {
    std::map<std::string, std::string> entries;     //file path & blob hashid
    const std::string INDEX_PATH = ".gitlite/index";

public:
    index(){ load();}

    void add_entry(std::string path , std::string hash);
    void rm_entry(const std::string& path);

    bool contains(const std::string& path);
    bool indentical(const std::string& path , Blob&);

    void write();
    void load();

};

#endif //GITLITE_INDEX_HPP