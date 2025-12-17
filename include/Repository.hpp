#ifndef GITLITE_REPOSITORY_HPP
#define GITLITE_REPOSITORY_HPP
#include<string>
#include<vector>
#include"ObjectDataBase.hpp"
#include"index.hpp"
#include "RefManager.hpp"

class Repository {
public:

    void init();

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

    void branch(const std::string& branchName);
    void rm_branch(const std::string& branchName);

    void reset(const std::string &commitId);

    void merge(const std::string &givenBranchName);


    void performThreeWayMerge(const std::map<std::string, std::string> &splitBlobs,
                              const std::map<std::string, std::string> &currentBlobs,
                              const std::map<std::string, std::string> &givenBlobs, index &idx, ObjectDatabase &db,
                              RefManager &refManager, const std::string &givenBranchName, const std::string &currentBranchName, const std::string &
                              currentHash, const std::string &givenHash);

    void writeBlobToWD(ObjectDatabase &db, const std::string &path, const std::string &blobHash);

    std::string findCommonAncestor(const std::string &hash1, const std::string &hash2);

    void addRemote(const std::string &name, const std::string &path);

    void rmRemote(const std::string &name);

    void traverseAndCopy(const std::string &start_hash, const std::string &end_hash, ObjectDatabase &local_db,
                         const std::string &remote_gitlite_path);

    void push(const std::string &remoteName, const std::string &remoteBranchName);

    void fetch(const std::string &remoteName, const std::string &remoteBranchName);

    void pull(const std::string &remoteName, const std::string &remoteBranchName);

    static std::string  getGitliteDir();
};

#endif //GITLITE_REPOSITORY_HPP