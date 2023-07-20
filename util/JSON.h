#include <iostream>
#include <vector>
#include <unordered_map>
#include <map>

#ifndef JSON_H
#define JSON_H

namespace ifdh_util_ns {

// this should be an abstract base and  5 subclasses for space efficiency, but not today...
class json {
    enum {_none, _string, _num, _list, _map} _shape;
    float natom;
    std::string patom;
    std::vector<json*> plist;
    std::map<std::string,json*> pmap;
public:
    json();
    json(std::string s);
    json(float n);
    json(std::vector<json*>);
    json(std::vector<std::string>);
    json(std::map<std::string,json*>);
    json(std::map<std::string,std::string>);
    bool is_none();
    bool is_string();
    bool is_num();
    bool is_list();
    bool is_map();
    json *&operator [](int);
    json *&operator [](std::string);
    float fval();
    std::string sval();
    ~json();
    void dump(std::ostream &s);
    std::string dumps();
    friend json *load_json(std::istream &s);
    friend json *load_json_map(std::istream &s);
    friend json *load_json_string(std::istream &s);
    friend json *load_json_list(std::istream &s);
    friend json *loads_json(std::string s);
};
json *load_json(std::istream &s);
json *loads_json(std::string s);

}
using namespace ifdh_util_ns;

#endif
