#include <exception>
#include <cctype>
#include <sstream>
#include "JSON.h"


// utilities for parsing:

static int linenum = 1;

// eat whitespace, keep track of linenum numbers for errors.
static void 
eatspace(std::istream &s) {
   while (isspace(s.peek())) {
       if (s.peek() == '\n') {
	   linenum++;
       }
       s.get();
   }
}

// raise a parse error..
static void
throwerror( int c ) {
    std::stringstream msg;
    msg << "Expected: '" << (char)c << "' at line "<< linenum ;
    throw std::runtime_error(msg.str());
}
static void
throwerror_s( const char *s ) {
    std::stringstream msg;
    msg << "Expected one of: \"" << s << "\" at line "<< linenum ;
    throw std::runtime_error(msg.str());
}

// consume an expected character and surrounding white space
static void 
eat(int c, std::istream &s) {
   eatspace(s);
   if (c) {
       if (s.peek() == c) {
	   s.get();
       } else {
	  throwerror(c);
       }
   }
}

void fjson::dump(std::ostream &s){s << fval; }
void fjson::load(std::istream &s){s >> fval; }
json *fjson::operator [](int){throw std::runtime_error("unimplemented"); return  new fjson; }
json *fjson::operator [](std::string){throw std::runtime_error("unimplemented"); new fjson; }
size_t fjson::hash() {std::hash<int> ihash; return ihash(fval); }
fjson::~fjson() {;}

void sjson::dump(std::ostream &s){s << '"'<< sval << '"'; }

void sjson::load(std::istream &s){
    eat('"', s);
    while (s.peek() != '"'){ 
        if (s.peek() == '\\') 
  	    s.get(); 
        sval.push_back(s.get()); 
    } 
    s.get(); 
}

size_t sjson::hash() { std::hash<std::string>shash; return shash(sval); }
json *sjson::operator [](int){throw std::runtime_error("unimplemented"); return new sjson; }
json *sjson::operator [](std::string){throw std::runtime_error("unimplemented");return new sjson; }
sjson::~sjson() {;}


void vjson::dump(std::ostream &s){
   s << "[";
   for( auto p = vec.begin(); p != vec.end(); p++) {
       if (p != vec.begin() )
           s << ",";
       (*p)->dump(s);
   }
   s << "]";
}

void vjson::load(std::istream &s){ 
   bool first=true;
   eat('[', s);
   eatspace(s);
   while (s.peek() != ']') {
       if (first) 
	   first = false;
       else {
	   eat(',', s);
           eatspace(s);
       }
       vec.push_back(load_json_i(s));
       eatspace(s);
   }
   s.get();
}
json *vjson::operator [](int i){return vec[i]; }
json *vjson::operator [](std::string){throw std::runtime_error("unimplemented"); new fjson; }
size_t vjson::hash() { 
   size_t res;
   for( auto p = vec.begin(); p != vec.end(); p++ ) 
      res = (res << 1) ^ (*p)->hash();
   return res;
}

vjson::~vjson() {
   for( auto p = vec.begin(); p != vec.end(); p++ ) 
       delete (*p);
}

mjson::~mjson() {
    for( auto p = map.begin(); p != map.end(); p++ ) {
        delete p->first;
        delete p->second;
    }
}

void mjson::dump(std::ostream &s) {
   s << "{";
   for( auto p = map.begin(); p != map.end(); p++ ) {
       if (p != map.begin())
           s << ", ";
       p->first->dump(s);
       s << ": ";
       p->second->dump(s);
   }
   s << "}";
}
void mjson::load(std::istream &s){ 
   json *k, *v;
   bool first=true;
   eat('{', s);
   eatspace(s);
   while (s.peek() != '}') {
       if (first) 
          first = false;
       else
	  eat(',', s);

       k = load_json_i(s);

       eat(':', s);

       v = load_json_i(s);
       map[k] = v;
       eatspace(s);
   }
   s.get();
}

json *mjson::operator [](int k)        { fjson ij(k); return map[&ij];}
json *mjson::operator [](std::string s){ sjson sj(s); return map[&sj];}
size_t mjson::hash() { 
   size_t res;
   for( auto p = map.begin(); p != map.end(); p++ ) 
      res = (res << 1) ^ p->first->hash() ^ p->second->hash();
   return res;
}

json *loads_json(std::string s) {
    std::stringstream ss;
    ss.str(s);
    linenum = 1;
    return load_json_i(ss);
}

json *load_json(std::istream &s) {
   linenum = 1;
   return load_json_i(s);
}

json *load_json_i(std::istream &s) {
   json *p;
   eatspace(s);
   switch(s.peek()) {
       case '0': case '1': case '2': case '3': case '4': 
       case '5': case '6': case '7': case '8': case '9':
	   p = new fjson();
	   break;
       case '"':
	   p = new sjson();
	   break;
       case '[':
	   p = new vjson();
	   break;
       case '{':
	   p = new mjson();
	   break;
       default:
	    throwerror_s("\"[{0123456789'");
       }
       p->load(s);
       return p;
}

json *conv_json(std::map<const char *, const char *> m) {
  mjson *m1 = new mjson;
  for( auto p = m.begin(); p != m.end(); p++ ) {
      m1->map[new sjson(p->first)] = new sjson(p->second);
  }
  return m1;
}

json *conv_json(std::map<const char *, double> m) {
  mjson *m1 = new mjson;
  for( auto p = m.begin(); p != m.end(); p++ ) {
      m1->map[new sjson(p->first)] = new fjson(p->second);
  }
  return m1;
}

json::json() {;}
json::~json() {;}

#ifdef UNITTEST
int
main() {
   std::map<const char *, const char *> md1 = {{"foo","bar"},{"baz","bleem"}};
   std::map<const char *, double> md2 = {{"foo",1},{"baz",2}};
   json *p = loads_json("[1,2,{3:4,5:6}]") ;
   p->dump(std::cout);
   delete p;
   std::cout << "\n";
   p = load_json(std::cin);
   p->dump(std::cout);
   conv_json(md1)->dump(std::cout);
   conv_json(md2)->dump(std::cout);
   delete p;
   return 0;
}
#endif
