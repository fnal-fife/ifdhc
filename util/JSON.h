#ifndef JSON_H
#define JSON_H
#include <iostream>
#include <exception>
#include <vector>
#include <map>
#include <memory>

class json;
class json_storage;
class json_none;
class json_str;
class json_num;
class json_list;
class json_dict;


class json_storage {
  public:
    virtual json & operator[]( int );
    virtual json & operator[]( json );
    virtual void dump(std::ostream &os) const;
    operator double();
    operator std::string(); 
};

class json {
    std::shared_ptr<json_storage> pval;
  public:
    json();
    json(json_storage *v);
    json(const char *);
    json(double);
    json(const json &);
    static json load(std::istream &is);
    static json loads(std::string s);
    std::string dumps() const;
    void dump(std::ostream &os) const;
    json & operator[]( int );
    json & operator[]( json );
    bool operator < (const json);
    operator std::string(); 
};

bool operator < (const json, const json);

class json_none : public json_storage {
  public:
    json_none();
    void dump(std::ostream &os) const;
    static json load(std::istream &is);
};

class json_str : public json_storage {
    std::string val;
  public:
    json_str(std::string s);
    json_str();
    void dump(std::ostream &os) const;
    static json load(std::istream &is);
    operator std::string();
    ~json_str();
};

class json_num : public json_storage {
    double val;
  public:
    json_num(double n);
    json_num();
    void dump(std::ostream &os) const; 
    static json load(std::istream &is); 
    operator double(); 
    ~json_num();
};

class json_list : public json_storage {
    std::vector<json> val;
  public:
    json_list();
    json_list(std::vector<json>);
    json_list(std::vector<std::string>);
    json & operator[]( int );
    void dump( std::ostream &os ) const; 
    static json load( std::istream &is );
    ~json_list();
};

class json_dict : public json_storage {
    std::map<json,json> val;
  public:
    json_dict();
    json_dict(std::map<std::string,json>);
    json_dict(std::map<std::string,std::string>);
    json & operator[]( json );
    void dump( std::ostream &os ) const;
    static json load( std::istream &is ); 
    ~json_dict();
};

#endif
