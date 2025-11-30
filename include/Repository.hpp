#ifndef GITLITE_REPOSITORY_HPP
#define GITLITE_REPOSITORY_HPP
#include<string>
#include<vector>
#include"ObjectDataBase.hpp"
#include"index.hpp"

class Repository {
public:

    void init();

    // void addRemote(std::vector<std::string>::const_reference value, std::vector<std::string>::const_reference basic_string);
    //
    // void rmRemote(std::vector<std::string>::const_reference value);
    //
    void add(std::string &);

    // void commit(std::vector<std::string>::const_reference value);
    //
    // void rm(std::vector<std::string>::const_reference value);
    //
    // void log();
    //
    // void globalLog();
    //
    // void find(std::vector<std::string>::const_reference value);
    //
    // void status();
    //
    // void checkoutBranch(std::vector<std::string>::const_reference value);
    //
    // void checkoutFile(std::vector<std::string>::const_reference value);
    //
    // void checkoutFileInCommit(std::vector<std::string>::const_reference value, std::vector<std::string>::const_reference basic_string);
    //
    // void branch(std::vector<std::string>::const_reference value);
    //
    // void rmBranch(std::vector<std::string>::const_reference value);
    //
    // void reset(std::vector<std::string>::const_reference value);
    //
    // void merge(std::vector<std::string>::const_reference value);
    //
    // void push(std::vector<std::string>::const_reference value, std::vector<std::string>::const_reference basic_string);
    //
    // void fetch(std::vector<std::string>::const_reference value, std::vector<std::string>::const_reference basic_string);
    //
    // void pull(std::vector<std::string>::const_reference value, std::vector<std::string>::const_reference basic_string);
    //
    static std::string  getGitliteDir();
};

#endif //GITLITE_REPOSITORY_HPP