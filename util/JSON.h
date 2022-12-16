#ifndef IFDH_JSON_H
#define IFDH_JSON_H
#include <vector>
#include <map>
#include <string>
#include <iostream>
#include <cstring>
#include <exception>
#include <memory>


class JSON {
public:
    class skipstream {
    public:
        virtual bool eof() = 0;
        virtual int getc() = 0;
        virtual void ungetc() = 0;
        char getnonblank();
    };
    class stringskipstream : public skipstream {
        std::string::iterator _p, _e;
    public:
        stringskipstream(std::string &s);
        bool eof();
        int getc();
        void ungetc();
    };
    class streamskipstream : public skipstream {
        std::istream &_s;
    public:
        streamskipstream(std::istream s);
        bool eof();
        int getc();
        void ungetc();
    };

    public:
    class ParseError : public std::exception {
        public:
        ParseError(skipstream &ss);
        const char *what();
    };

    class JSONdata;

    typedef std::map<JSONdata&, JSONdata&, JSONcmp> JSONmap;
    typedef std::vector<JSONdata&> JSONvec;

    class JSONdata {
         public: 
         enum { fval, sval, lval, mval, nval} which;
         virtual void dump(std::ostream &s) const = 0;
    };

    class JSONdata_float : public JSONdata {
         float floatval;
         void dump(std::ostream &s) const;
         bool cmp(const JSONdata_float &);
         JSONdata_float(float);
    }

    class JSONdata_string : public JSONdata {
         std::shared_ptr<std::string> strp;
         void dump(std::ostream &s) const;
         bool cmp(const JSONdata_string &);
         JSONdata_string(std::string);
    }
    }
    class JSONdata_list : public JSONdata {
         std::shared_ptr<JSONvec> list;
         void dump(std::ostream &s) const;
         bool cmp(const JSONdata_list &);
         JSONdata_list();
         void append(JSONdata &);
    }
    class JSONdata_map : public JSONdata {
         std::shared_ptr<JSONmap> map;
         void dump(std::ostream &s) const;
         bool cmp(const JSONdata_map &);
         JSONdata_map();
         void append(JSONdata &, JSONdata &);
    }

    struct JSONcmp {
    bool operator ()(const JSONdata &x,const  JSONdata &y);
    };

    std::shared_ptr<const char> getstr(skipstream &dataobj, char quote);

    std::shared_ptr<JSONvec> getvec(skipstream &dataobj);

    std::shared_ptr<JSONmap> getmap(skipstream &dataobj);

    JSONdata parsejson(skipstream &dataobj);
};
#endif
