#ifndef GITLITE_OBJECTS_HPP
#define GITLITE_OBJECTS_HPP
#include <string>
#include <vector>


//GitLiteObject class is the base class for all git classes
class GitLiteObject {
protected:
    std::string hashid;
    unsigned obj_type{};
public:
    std::string get_hashid(){return hashid;}
    void set_hash(std::string);

    GitLiteObject() = default;
    virtual ~GitLiteObject() = default;

    virtual std::string serialize() = 0;
    virtual void deserialize(const std::string& data) = 0;
};

//metadata for commit obj
struct MetaData {
    std::string message;
    std::string timestamp;

    explicit MetaData(std::string  , std::string);
};

//Commit class
class Commit:public GitLiteObject {
private:
    MetaData Commit_Metadata;
    std::vector<std::string> Father_Commit;
    std::vector<std::string> Blobs;
public:
    Commit();
    //explicit Commit(MetaData , std::string , std::string);
    std::string serialize() override;
    void deserialize(const std::string &data) override;
};

class Blob:public GitLiteObject {
private:
    std::string content;
public:
    Blob() = default;

    explicit Blob(std::string fileContent) : content(std::move(fileContent)) {}

    std::string getContent() const { return content; }
    void set_content(std::string  _content) {content = std::move(_content);}

    std::string serialize() override;
    void deserialize(const std::string &data) override;
};

#endif //GITLITE_OBJECTS_HPP