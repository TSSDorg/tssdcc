#include "flat.h"
#include "tssd.h"

using namespace std;

struct Course {
    string title;
    string teacher;
    float  score;
};

struct Contact {
    string name;
    string relation;
    string phone;
    string address;
};

class Student : public tssd::Flatable {
    uint64_t ID;
public:
    string   name;
    int16_t  age;
    bool     IsMale;

    vector<Contact> contacts;
    map<string, Course> courses;

    Student(uint16_t id) : ID(id) {}
    void print() {
        cout << "Student ID" << ID << ", name:" << name << endl;
    }
};

vector<byte> ToBytes()
{

}
