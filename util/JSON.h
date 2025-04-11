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
  // abstract base class for storing different types of JSON blobs:
  // subclassed later for numbers, strings, lists and dictionaries
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
    virtual bool has_item(json&) {return false;}
    virtual int  size() { return 0; }
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
    json & operator[]( const char *cs ) { return (*this)[json(cs)]; }
    json & operator[]( std::string s ) { return (*this)[json(s.c_str())]; }
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
    virtual bool has_item(json& j) {return pval->has_item(j);}
    virtual bool has_item(const char *cs) {json jcs(cs); return pval->has_item(jcs);}
    virtual int  size() {return pval->size();}
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
    virtual int size() { return val.size(); }
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
    virtual int size() { return 1; }
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
    virtual int size() { return 1; }
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
    virtual bool has_item(json& j) const {return j.is_number() && (double)j < val.size();}
    virtual int  size() {return val.size();}
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
    virtual bool has_item(json& j) {try{ val.at(j); return true;}  catch(const std::out_of_range& ex) {return false; }}
    virtual int  size() {return val.size();}
};

}
using namespace ifdh_util_ns;
#endif
