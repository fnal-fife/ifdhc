#ifndef JSON_H
#define JSON_H
#include <iostream>
#include <exception>
#include <vector>
#include <map>
#include <memory>

namespace ifdh_util_ns {
class json;
class json_storage;
class json_null;
class json_str;
class json_num;
class json_list;
class json_dict;


class json_storage {
  public:
    virtual json & operator[]( int );
    virtual json & operator[]( json );
    virtual void dump(std::ostream &os) const;
    virtual operator double();
    virtual operator bool();
    virtual operator std::string(); 
    virtual std::vector<json> keys();
    virtual bool is_null() {return false;}
    virtual bool is_list() {return false;}
    virtual bool is_dict() {return false;}
    virtual bool is_bool() {return false;}
    virtual bool is_string() {return false;}
    virtual bool is_number() {return false;}
};

class json {
    std::shared_ptr<json_storage> pval;
  public:
    json();
    virtual ~json();
    json(json_storage *v);
    json(const char *);
    json(double);
    json(const json &);
    json& operator=(const json &);
    static json load(std::istream &is);
    static json loads(std::string s);
    std::string dumps() const;
    void dump(std::ostream &os) const;
    json & operator[]( int );
    json & operator[]( json );
    bool operator < (const json);
    operator std::string(); 
    operator double(); 
    operator int(); 
    operator bool();
    std::vector<json> keys();
    virtual bool is_null()   {return pval->is_null();}
    virtual bool is_list()   {return pval->is_list();}
    virtual bool is_dict()   {return pval->is_dict();}
    virtual bool is_string() {return pval->is_string();}
    virtual bool is_number() {return pval->is_number();}
};

bool operator < (const json, const json);

class json_null : public json_storage {
  public:
    json_null();
    void dump(std::ostream &os) const;
    static json load(std::istream &is);
    virtual bool is_null() {return true;}
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
    virtual bool is_string() {return true;}
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
    virtual bool is_number() {return true;}
};
class json_bool : public json_storage {
    bool val;
  public:
    json_bool(bool n);
    json_bool();
    void dump(std::ostream &os) const; 
    static json load(std::istream &is); 
    operator bool(); 
    ~json_bool();
    virtual bool is_bool() {return true;}
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
    virtual bool is_list() {return true;}
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
    std::vector<json> keys();
    ~json_dict();
    virtual bool is_dict() {return true;}
};

}
using namespace ifdh_util_ns;
#endif
