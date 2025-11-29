#include "Objects.hpp"
#include<string>
#include <sstream>
#include <utility>
#include "Def.hpp"
#include "Utils.h"
#include "GitliteException.h"

void GitLiteObject::set_hash(std::string _hash) {
    hashid = std::move(_hash);
}

MetaData::MetaData(std::string _message , std::string _timestamp) : message(std::move(_message)),timestamp(std::move(_timestamp)){}

Commit::Commit() : Commit_Metadata("initial commit","Thu Jan 01 00:00:00 1970 +0000") {
    obj_type = Git_Commit;
}

std::string Commit::serialize() {
    std::stringstream body_ss;

    //1. serialize the blobs info
    body_ss << "blobs_of_commit:\n";
    for (const auto& obj: this->Blobs) {
        body_ss << obj << "\n";
    }

    //2. serialize father commit
    body_ss << "father_commit:\n";
    if (!this->Father_Commit.empty()) {
        for (const auto& obj : this->Father_Commit) {
            body_ss << obj << "\n";
        }
    }

    //3. serialize meta_message
    body_ss <<"\n"<<this->Commit_Metadata.message<<"\n"<<this->Commit_Metadata.timestamp;

    //final string is "commit + size + \0 + body"
    std::string body = body_ss.str();
    std::stringstream final_ss;

    final_ss << "commit " << body.size() << '\0' << '\n';

    final_ss << body;

    return final_ss.str();
}

void Commit::deserialize(const std::string &content) {
    this->Blobs.clear();
    this->Father_Commit.clear();
    this->Commit_Metadata.message.clear();
    this->Commit_Metadata.timestamp.clear();

    //read all
    std::stringstream ss(content);
    std::string line;
    std::vector<std::string> all_lines;
    while (std::getline(ss, line)) {
        all_lines.push_back(line);
    }
    if (all_lines.empty()) return;

    // read Timestamp
    this->Commit_Metadata.timestamp = all_lines.back();
    all_lines.pop_back();



    //left the head & blobs & father & message
    size_t idx = 1;

    // "blobs_of_commit:"
    if (idx < all_lines.size() && all_lines[idx] == "blobs_of_commit:") {
        idx++;
        while (all_lines[idx] != "father_commit:") {
            Blobs.emplace_back(all_lines[idx]);
            ++idx;
        }
    }else {
        throw GitliteException("Illegle Form Of Commit File!");
    }


    //read father_commit
    if (idx < all_lines.size() && all_lines[idx] == "father_commit:") {
        idx++;
        while (!all_lines[idx].empty())
        {
            Father_Commit.emplace_back (all_lines[idx]);
            ++idx;
        }
    }else {
        throw GitliteException("Illegle Form Of Commit File!");
    }

    // read message (剩余的所有行)
    while (idx < all_lines.size()) {
        if (!this->Commit_Metadata.message.empty()) {
            this->Commit_Metadata.message += "\n";
        }
        else {
            this->Commit_Metadata.message += all_lines[idx];
        }
        idx++;
    }
}


