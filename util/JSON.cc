
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
   return x.cmp(y);
}
   if (x.which == JSON::JSONdata::fval) return x.floatval < y.floatval;
   if (x.which == JSON::JSONdata::sval) return strcmp(&(*x.strvalue), &(*y.strvalue)) < 0;
   if (x.which == JSON::JSONdata::lval) return x.list < y.list;
   if (x.which == JSON::JSONdata::mval) return x.map < y.map;
   return false;
}        

void JSON::JSONdata_float.dump(std::ostream &s) { s << floatval; }
bool JSON::JSONdata_float.cmp(JSON::JSONdata_float &df) { return floatval < df.floatval; }
JSON::JSONdata_float.JSONdata_float(float f):which(JSON::JSONdata::fval),floatval(f) {;}

void JSON::JSONdata_string.dump(std::ostream &s) { s << *strp; }
bool JSON::JSONdata_string.cmp(JSON::JSONdata_string &df) { return *strp < *df.strp; }
JSON::JSONdata_string.JSONdata_string(std::string &s):which(JSON::JSONdata::sval),strp(s) {;} // need make_shared?
JSON::JSONdata_list.dump(std::ostream &s) {
   long i;
   s << "[";
   for(i = 0; i < (long)list->size()-1; i++) {
       (*list)[i].dump(s);
       s << ", ";
   }
   (*list)[i].dump(s);
   s << "]";
}
JSON::JSONdata_list.cmp(JSONdata_list &l2) {
    if (list->size() && l2.list->size()) {
        cmp(list[0], l2->list[0]);
    }
}
JSON::JSONdata_list.JSONdat_list() {
    list(mkshared<JSONdata_vec>);
}
JSON::JSONdata_list.append(JSONdata &d) {
    list->push_back(d);
}

JSON::JSONdata_map.dump(std::ostream &s) {
   const char *sep = "";
   s << "{";
   for(JSON::JSONmap::iterator it = map->begin(); it != map->end(); it++ ) {
       s << sep;
       it->first.dump(s);
       s << ": ";
       it->second.dump(s);
       sep = ", ";
   }
   s << "}";
}
bool
JSON::JSONdata_map.cmp(JSONdata_map &){
    return 
}
JSON::JSONdata_map.JSONdata_map(){
}
void
JSON::JSONdata_map.append(JSONdata &k, JSONdata &v){
}
    class JSONdata_map : public JSONdata {
         std::shared_ptr<JSONmap> map;
         void dump(std::ostream &s) const;
         bool cmp(const JSONdata_map &);
         JSONdata_map();
         void append(JSONdata &, JSONdata &);
    }

JSONdata_string
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
   return JSONdata_string(res);
}

std::shared_ptr<JSON::JSONvec> JSON::getvec(skipstream &dataobj) {
    std::shared_ptr<JSON::JSONvec> vres = std::make_shared<JSON::JSONvec>();
    int c;
    vres->clear();
    // already saw the '['...
    do {
        vres->push_back(parsejson(dataobj));
        c = dataobj.getnonblank();
    } while (c == ',');
    if ( c != ']') {
        throw ParseError(dataobj);
    }
    return vres;
}

std::shared_ptr<JSON::JSONmap> JSON::getmap(skipstream &dataobj) {
   std::shared_ptr<JSON::JSONmap> mres = std::make_shared<JSON::JSONmap>();
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
       s << floatval;
   if (which == JSON::JSONdata::sval) 
       s << '"' << strvalue << '"' ;; // XXX needs escaping for quotes...
   if (which == JSON::JSONdata::lval) { 
       long i;
       s << "[";
       for(i = 0; i < (long)list->size()-1; i++) {
           (*list)[i].dump(s);
           s << ", ";
       }
       (*list)[i].dump(s);
       s << "]";
   }
   if (which == JSON::JSONdata::mval)  {
       const char *sep = "";
       s << "{";
       for(JSON::JSONmap::iterator it = map->begin(); it != map->end(); it++ ) {
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
        res.strvalue = getstr(dataobj, c);
        break;
    case '{':
        res.which = JSON::JSONdata::mval;
        res.map = getmap(dataobj);
        break;
    case '[':
        res.which = JSON::JSONdata::lval;
        res.list = getvec(dataobj);
        break;
    case '0': case '1': case '2': case '3':
    case '4': case '5': case '6': case '7':
    case '8': case '9':
        res.which = JSON::JSONdata::fval;
        res.floatval = c - '0';
        c = dataobj.getc();
        while (isdigit(c))  {
           res.floatval = res.floatval*10 + c - '0';
           c = dataobj.getc();
        } 
        if ( '.' == c ) {
            float m = 0.1;
            c = dataobj.getc();
            while (isdigit(c))  {
                res.floatval = res.floatval + m * (c - '0');
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
