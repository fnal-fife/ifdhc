
#include <vector>
#include <map>
#include <string>
#include <iostream>
#include <cstring>
#include <exception>
#include <malloc.h>

#include "JSON.h"


char JSON::skipstream::getnonblank() {
    char c = getc();
    while( isspace(c) ) {
        c = getc();
    }
    return c;
}
JSON::stringskipstream::stringskipstream(std::string &s) {
   _p = s.begin(); _e = s.end();
}
bool JSON::stringskipstream::eof()  { return _p == _e; }
int JSON::stringskipstream::getc() { return *_p++; }
void JSON::stringskipstream::ungetc() { --_p; }

JSON::streamskipstream::streamskipstream(std::istream s) : _s(s) {;}
bool JSON::streamskipstream::eof() { return _s.eof(); }
int JSON::streamskipstream::getc() { return _s.get(); }
void JSON::streamskipstream::ungetc() { _s.unget(); }

JSON::ParseError::ParseError(skipstream &ss) { ss.ungetc(); }
const char *JSON::ParseError::what() { return "JSON parse error"; }

bool JSON::JSONcmp::operator ()(const JSON::JSONdata &x, const JSON::JSONdata &y) {
   if (x.which != y.which)        return x.which < y.which;
   if (x.which == JSON::JSONdata::fval) return x.u.floatval < y.u.floatval;
   if (x.which == JSON::JSONdata::sval) return strcmp(x.u.strvalue, y.u.strvalue) < 0;
   if (x.which == JSON::JSONdata::lval) return (long int)x.u.list < (long int)(x.u.list);
   if (x.which == JSON::JSONdata::mval) return (long int)x.u.map < (long int)(x.u.map);
   return false;
}        

JSON::JSONdata::JSONdata() {
   // std::cout << "JSONdata constructor: " << long(this) << "\n";
   which = JSON::JSONdata::nval;
}

//JSON::JSONdata::JSONdata(const JSONdata &x) {
//   std::cout << "JSONdata copy constructor: " << long(this) << "\n";
//   which = x.which;
//   if (x.which == JSON::JSONdata::fval) u.floatval = x.u.floatval;
//   if (x.which == JSON::JSONdata::sval) u.strvalue = strdup(x.u.strvalue);
//   if (x.which == JSON::JSONdata::lval) u.list = new JSONvec(*x.u.list);
//   if (x.which == JSON::JSONdata::mval) u.map = new JSONmap(*x.u.map);
//}
//JSON::JSONdata::~JSONdata() {
//   std::cout << "JSONdata destructor( " <<  long(this) << ")\n";
//
//   if (which == JSON::JSONdata::sval) {
//      std::cout << "I would free: " <<  long(u.strvalue) << ":" <<u.strvalue << "\n";
//      //free((void*)u.strvalue);
//   }
//   if (which == JSON::JSONdata::lval)  {
//      std::cout << "I would free list: " << long(u.list) << ":" <<  u.list->size() << "\n";
//      // delete u.list;
//   }
//   if (which == JSON::JSONdata::mval)  {
//      std::cout << "I would free map: "<< long(u.map) << ":"  <<  u.map->size() << "\n";
//      // delete u.map;
//   }
//}

const char *
JSON::getstr(skipstream &dataobj, char quote) {
   
   int c;
   std::string res;
   c = dataobj.getc();
   do { 
       if ( c == '\\' ) {
          c = dataobj.getc();
       }
       res.push_back(c);
       c = dataobj.getc();
   } while ( c != quote );
   return  strdup(res.c_str());
}

JSON::JSONvec *JSON::getvec(skipstream &dataobj) {
    JSON::JSONvec *vres = new JSON::JSONvec;
    int c;
    (*vres).clear();
    // already saw the '['...
    do {
        (*vres).push_back(parsejson(dataobj));
        c = dataobj.getnonblank();
    } while (c == ',');
    if ( c != ']') {
        throw ParseError(dataobj);
    }
    return vres;
}

JSON::JSONmap *JSON::getmap(skipstream &dataobj) {
   JSON::JSONmap *mres = new JSON::JSONmap;
   JSON::JSONdata k, v;
    (*mres).clear();
   int c;
   do {
       k = parsejson(dataobj);
       c = dataobj.getnonblank();
       if (c != ':')
           throw ParseError(dataobj);
       v = parsejson(dataobj);
       
       (*mres)[k] = v;
       c = dataobj.getnonblank();
   } while( c == ',' );
   return mres;
}

void 
JSON::JSONdata::dump(std::ostream &s) const {
    
   if (which == JSON::JSONdata::fval) 
       s << u.floatval;
   if (which == JSON::JSONdata::sval) 
       s << '"' << u.strvalue << '"' ;; // XXX needs escaping for quotes...
   if (which == JSON::JSONdata::lval) { 
       long i;
       s << "[";
       for(i = 0; i < (long)u.list->size()-1; i++) {
           (*u.list)[i].dump(s);
           s << ", ";
       }
       (*u.list)[i].dump(s);
       s << "]";
   }
   if (which == JSON::JSONdata::mval)  {
       const char *sep = "";
       s << "{";
       for(JSON::JSONmap::iterator it = u.map->begin(); it != u.map->end(); it++ ) {
           s << sep;
           it->first.dump(s);
           s << ": ";
           it->second.dump(s);
           sep = ", ";
       }
       s << "}";
   }
   if (which == JSON::JSONdata::nval) {
       s<< "None";
   }
}

JSON::JSONdata JSON::parsejson(skipstream &dataobj) {
    JSON::JSONdata res;
    int c = dataobj.getnonblank();
    res.which = JSON::JSONdata::nval;
      
    switch(c) {
    case '\'':
    case '"':
        res.which = JSON::JSONdata::sval;
        res.u.strvalue = getstr(dataobj, c);
        break;
    case '{':
        res.which = JSON::JSONdata::mval;
        res.u.map = getmap(dataobj);
        break;
    case '[':
        res.which = JSON::JSONdata::lval;
        res.u.list = getvec(dataobj);
        break;
    case '0': case '1': case '2': case '3':
    case '4': case '5': case '6': case '7':
    case '8': case '9':
        res.which = JSON::JSONdata::fval;
        res.u.floatval = c - '0';
        c = dataobj.getc();
        while (isdigit(c))  {
           res.u.floatval = res.u.floatval*10 + c - '0';
           c = dataobj.getc();
        } 
        if ( '.' == c ) {
            float m = 0.1;
            c = dataobj.getc();
            while (isdigit(c))  {
                res.u.floatval = res.u.floatval + m * (c - '0');
                m = m * 0.1;
                c = dataobj.getc();
            }
        }
        dataobj.ungetc();
        break;
    default:
        throw ParseError(dataobj);
    }
    return res;
}

#ifdef UNITTEST
int
main() {
    std::string s1 = "['a','b','c']";
    JSON j;
    JSON::JSONdata res;

    JSON::stringskipstream ss1(s1);
    res = j.parsejson(ss1);
    res.dump(std::cout);
    std::cout << "\n=====\n";
    std::cout.flush();
    std::string s2 = "{'e':'f','g':'h'}";
    JSON::stringskipstream ss2(s2);
    res = j.parsejson(ss2);
    res.dump(std::cout);
    std::cout << "\n=====\n";
    std::cout.flush();
    std::string s3 = "{'i':['j','k','l'],'m':'n'}";
    JSON::stringskipstream ss3(s3);
    res = j.parsejson(ss3);
    res.dump(std::cout);
}
#endif
