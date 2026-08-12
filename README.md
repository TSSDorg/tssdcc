# TSSDcc
[TSSD](http://https://github.com/TSSDorg/TSSD-Spec "TSSD") is an open binary data for exchange or storage.
tssdgo implement TSSD with C++, you can read, write and print TSSD data with this library package easily.
## features

- **API simple** **effective**: parse with reflect once, run without reflect
- **schema validation**: validate schema to prevent crash caused by unmashaling unmatched data
- **struct migration support**: support receive old struct data and migration to the latest version
- **less depenency**: depend third library google-test only, which for test. but require C++ reflection feture(GCC-16.1)



## quick start

```
/* 3 steps to compile(you need GCC-16.1 support), now test on Linux only
*  1). cd tssdcc/tssd;  make;
*  2). cd tssdcc/examples/transfer; make;
*  3)  run server: ./build/linux/debug/bin/program 192.168.6.188 8080
*  4). run client: ./build/linux/debug/bin/program 192.168.6.188 8080 0

*  if you want run UTs, you need install gtest
*  4). cd tssdc/tssd;  make test;
*/

// the complete demo code: examples/transfer/main.cc

// define a real class with string/vector/map/list/shared_ptr/unique_ptr
// just derived tssd::Flatable and implment Family(), Version()
class Student : public tssd::Flatable {
    std::uint64_t ID;
public:
    std::string   name;
    std::int16_t  age;
    bool     IsMale;

    std::vector<Contact> contacts;
    std::map<std::string, Course> courses;
    std::list<Paper> papers;

    Student(std::uint16_t id=0) : ID(id) {}
    void print() {
        std::cout << "Student ID" << ID << ", name:" << name << std::endl;
        for (const auto &it : contacts) {
             std::cout << "contact name:" << it.name << ", address:" << it.address << std::endl;
        }
        for (const auto& [key, value] : courses) {
             std::cout << "course title:" << value.title << ", teacher:" << value.teacher << std::endl;
        }
        for (const auto &it : papers) {
             std::cout << "paper title:" << it.title << ", tags:" << it.tags[0] << std::endl;
        }
    }

    std::string Family() const override {
        return "StudentFamily";
    }
    std::string Version() const override {
        return "StudentV1";
    }
};

// another real class need send to server
class Request : public tssd::Flatable {
public:
    std::int16_t fid;
    std::string types;
    std::string tid;
    std::string Family() const override {
        return "RequestFamily";
    }
    std::string Version() const override {
        return "RequestV1";
    }
};

class SocketReader : public tssd::Reader {
    int sockfd_;
    int flags_;
public:
    SocketReader(int sockfd, int flags=0) : sockfd_(sockfd), flags_(flags) {}
    int Read(void *dest, std::size_t numb) const override {
        auto n = recv(sockfd_, dest, numb, flags_);
        std::cout << " recv " << n << " bytes" << std::endl;
        return n;
    }
};

// demo to receive an object from client
int recvRequest(int sockfd)
{
    tssd::Manager::Register<Request>();
    SocketReader socketReader(sockfd);
    Request request;
    if (auto ret = tssd::Manager::Read(socketReader, request)) {
        printf("recv request failure: %d\n", ret);
    }
    printf("request: %d %s %s\n", request.fid, request.tid.c_str(), request.types.c_str());
    return request.fid;
}

// demo to send an object to client
bool sendStudent(int sockfd)
{
    Student student;
    student.name = "George W. Bush";
    student.age = 25;
    student.IsMale = true;

    tssd::Manager::Register<Student>();
    SocketWriter socketWriter(sockfd);
    auto ret = tssd::Manager::Write(socketWriter, student);
    if (ret != tssd::OK) {
        printf("Write data failure: %d\n", ret);
        return false;
    }
    return true;
}
```
