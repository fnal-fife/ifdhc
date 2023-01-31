#include <iostream>
#include <vector>
#include <unordered_map>
#include <map>

class json {
public:
    json();
    virtual void dump(std::ostream &s) = 0;
    virtual void load(std::istream &s) = 0;
    virtual json *operator [](int) = 0;
    virtual json *operator [](std::string) = 0;
    virtual size_t hash() = 0;
    virtual ~json();
};

class fjson: public json { 
    double fval;
public:
    fjson() : fval(0) {;}
    fjson(double f) : fval(f) {;}
    void dump(std::ostream &s);
    void load(std::istream &s);
    json *operator [](int);
    json *operator [](std::string);
    size_t hash();
    virtual ~fjson();
};

class sjson: public json { 
    std::string sval;
public:
    sjson():sval(""){;}
    sjson(std::string s):sval(s){;}
    sjson(char *s):sval(s){;}
    void dump(std::ostream &s);
    void load(std::istream &s);
    json *operator [](int);
    json *operator [](std::string);
    size_t hash();
    virtual ~sjson();
};

class vjson: public json { 
    std::vector<json *> vec;
public:
    void dump(std::ostream &s);
    void load(std::istream &s);
    json *operator [](int);
    json *operator [](std::string);
    size_t hash();
    virtual ~vjson();
};

extern json *conv_json(std::map<const char *, const char *>);
extern json *conv_json(std::map<const char *, int>);

class mjson: public json { 
    std::unordered_map<json *,json *> map;
public:
    void dump(std::ostream &s);
    void load(std::istream &s);
    json *operator [](int);
    json *operator [](std::string);
    size_t hash();
    virtual ~mjson();
    friend json *conv_json(std::map<const char *, const char *>);
    friend json *conv_json(std::map<const char *, double>);
};

json *load_json(std::istream &s);
json *loads_json(std::string s);
json *load_json_i(std::istream &s);
size_t hash(json &j);
