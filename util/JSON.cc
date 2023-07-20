#include <exception>
#include <cctype>
#include <sstream>
#include "JSON.h"

void 
assert(bool b) { 
    if(!b) 
        throw std::runtime_error("assertion error");
}

bool
is_in(char c, const char *p) {
  while(p && *p) {
     if (c == *p)
        return true;
     p++;
  }
  return false;
}

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
static bool 
eat(int c, std::istream &s) {
   eatspace(s);
   if (c) {
       if (s.peek() == c) {
	   s.get();
           return true;
       } else {
           throwerror( c );
       }
   }
   return false;
}

json::json(){_shape = _none; patom = "";}
json::json(std::string s){_shape = _string; patom = s;}
json::json(float n){_shape = _num; natom = n;}
json::json(std::vector<json*> l1){_shape = _list; plist = l1;}
json::json(std::map<std::string,json*>m1){_shape = _map; pmap = m1;}
bool json::is_none()                   { return _shape == _none;}
bool json::is_string()                 { return _shape == _string;}
bool json::is_num()                    { return _shape == _num;}
bool json::is_list()                   { return _shape == _list;}
bool json::is_map()                    { return _shape == _map;}
json *&json::operator [](int i)        {assert(is_list()); return plist[i];}
json *&json::operator [](std::string s){assert(is_map()); return pmap[s];}
std::string json::sval()               {assert(is_string()); return patom;}
float json::fval()                     {assert(is_num()); return natom;}
json::~json(){;} //XXX -- recursvely delete

json::json(std::vector<std::string> l1){
    _shape = _list; 
    
    for( auto p = l1.begin(); p != l1.end(); p++ ) {
        plist.push_back(new json(*p));
    }
}
json::json(std::map<std::string,std::string>m1){
    _shape = _map; 
    for( auto p = m1.begin(); p != m1.end(); p++ ) {
       pmap.insert( std::pair<std::string, json *> ( p->first, new json(p->second)));
    }
}

void
json::dump(std::ostream &s) {
    bool first=true; 
    switch(_shape) {
    case _none: s << "None"; break;
    case _string: s << '"' << patom << '"'; break;
    case _num: s << natom; break;
    case _list: 
          s<< "["; 
          for( auto p = plist.begin(); p != plist.end(); p++) { 
             if(!first) 
                s << ", "; 
             first=false; 
             (*p)->dump(s); 
         } 
         s << "]"; 
         break;
    case _map: 
         s<< "{"; 
         for( auto p = pmap.begin(); p != pmap.end(); p++) { 
             if (!first) 
                 s<< ", "; 
             first=false;  
             s << '"' << p->first << '"'; 
             s<<":";
             p->second->dump(s);
          }  
          s << "}"; 
          break;
   }
}

json* 
load_json_num(std::istream &s) {
   std::string res; // XXX handle backslash quotes...
   while( !s.eof() && (isdigit(s.peek()) || '.' == s.peek()) ) {
      res.push_back(s.get());
   }
   return new json(atof(res.c_str()));
}
json* 
load_json_string(std::istream &s) {
   std::string res; // XXX handle backslash quotes...
   s.get(); // eat the quote
   while( !s.eof() && s.peek() != '"' ) {
      res.push_back(s.get());
   }
   s.get(); // eat the quote
   return new json(res);
}

json*
load_json_list(std::istream &s) {
   std::vector<json *> res;    
   eat('[',s);
   eatspace(s);
   while ( !s.eof() && s.peek() != ']' ) {
       res.push_back(load_json(s));
       eatspace(s);
       if ( s.peek() == ',') {
           eat(',',s);
       } else {
           if (s.peek() != ']') {
               throwerror_s(",]");
           }
       }
   }
   return new json(res);
}

json*
load_json_map(std::istream &s) {
   json *j1, *j2;
   std::map<std::string, json *> res;    
   eat('{',s);
   eatspace(s);
   while ( !s.eof() && s.peek() != '}' ) {
       j1 = load_json(s);
       eat(':', s);
       j2 = load_json(s);
       eatspace(s);

       res.insert(std::pair<std::string, json *>(j1->patom, j2));
       
       if ( s.peek() == ',') {
           eat(',',s);
       } else { 
           if (s.peek() != '}') {
               throwerror_s(",}");
           }
       }
   }
   return new json(res);
}

json *
load_json(std::istream &s) {
   json * res;
   eatspace(s);
   switch(s.peek()) {
   case '{':
      return load_json_map(s);
   case '[':
      return load_json_list(s);
   case '"':
      res = load_json_string(s);
      return res;
   case '0': case '1': case '2': case '3': case '4':
   case '5': case '6': case '7': case '8': case '9':
      return load_json_num(s);
   }
   return 0;
}

json *loads_json(std::string s){
   std::stringstream ss(s.c_str());
   return load_json(ss);
}

#ifdef UNITTEST
int
main() {
   std::string t1("[1, 22, 333]");
   std::string t2("{\"a\": 1, \"bb\":22, \"ccc\": 333}");
   std::string t3("{\"a\": \"x\", \"bb\":\"y\" , \"ccc\": 333}");
   json *r;
   r = loads_json(t1);
   std::cout << "t1: " << t1 << "\n";
   std::cout << "t1out: " ;
   r->dump(std::cout);
   std::cout << "\n";
   r = loads_json(t2);
   std::cout << "t2: " << t2 << "\n";
   std::cout << "t2out: " ;
   r->dump(std::cout);
   std::cout << "\n";
   r = loads_json(t3);
   std::cout << "t3: " << t3 << "\n";
   std::cout << "t3out: " ;
   r->dump(std::cout);
   std::cout << "\n";

   std::cout << "from std::vector<std::string>\n";

   std::vector<std::string> vs;
   vs.push_back("s0");
   vs.push_back("s1");
   vs.push_back("s2");
   r = new json(vs);
   r->dump(std::cout);

   std::map<std::string, std::string> mss;
   mss.insert( std::pair<std::string, std::string>( "k0", "v0"));
   mss.insert( std::pair<std::string, std::string>( "k1", "v1"));
   mss.insert( std::pair<std::string, std::string>( "k2", "v2"));
   r = new json(mss);
   r->dump(std::cout);
}
#endif
