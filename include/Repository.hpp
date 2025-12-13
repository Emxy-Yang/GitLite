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

    void commit(std::string &);

    void rm(std::string &);

    void log();

    void globalLog();

    void find(const std::string& message);

    void status();


    void checkoutFile(const std::string& fileName);
    void checkoutBranch(const std::string& branchName);
    void checkoutFileInCommit(const std::string& commitId, const std::string& fileName);
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