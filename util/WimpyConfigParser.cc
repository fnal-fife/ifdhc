#include "WimpyConfigParser.h"
#include <fstream>
#include <iostream>

namespace ifdh_util_ns {

WimpyConfigParser::WimpyConfigParser() { ; }

void
WimpyConfigParser::read(std::string filename) {
   std::ifstream s(filename.c_str());
   readfp(s);
   s.close();
}


void
WimpyConfigParser::readfp(std::istream &s) {
    std::string cur_section(""), line, cur_val, cur_key;
    size_t p, p2, p3; // string positions
    add_section(cur_section);
    bool have_partial = false;
    while( !s.eof() && !s.fail()) {
       getline(s, line);
       // deal with comments
       p = line.find_first_not_of(" \t");
       if (line == "" || line[p] == '#' || line[p] == ';' ) {
          // it is an all comment or blank line
          continue;
       }
       p = line.find('#');
       if (p != std::string::npos && line[p-1] != '\\') {
          // it has a comment on the end, trim it
          line = line.substr(0,p);
       }
       if (line[0] == '[') {
          if (have_partial) {
              set(cur_section, cur_key, cur_val);
              cur_val = cur_key = "";
              have_partial = false;
          }
          // section header
          p = line.find(']');
          cur_section = line.substr(1,p-1);
          add_section(cur_section);
       } else if (line[0] == ' ') {
          //continued line
          p = line.find_first_not_of(" \t", 0);
          cur_val += '\n';
          cur_val += line.substr(p);
       } else  {
          //should be name = value
          if (have_partial) {
              set(cur_section, cur_key, cur_val);
              cur_val = "";
              cur_key = "";
              have_partial = false;
          }
          p = line.find('=');
          if (p != std::string::npos) {
              p2 = line.find_last_not_of(" \t", p-1);
              cur_key = line.substr(0,p2+1);
              p = line.find_first_not_of(" \t", p+1);
              p3 = line.find_last_not_of(" \t");
              if (p != std::string::npos && p3 != std::string::npos && p3 >= p) {
                  cur_val = line.substr(p,p3 - p+1);
              } else {
                  cur_val = "";
              }
              have_partial = true;
          } else {
              std::cerr << "Error! line: " << line << std::endl;
          }
       }
    }
    if (have_partial) {
	set(cur_section, cur_key, cur_val);
    }
    return;
}

void
WimpyConfigParser::dump() {
   std::map<std::string, std::map<std::string, std::string> >::iterator si;
   std::map<std::string, std::string> ::iterator di;

   try {
	   std::cout << "# ------ config dump start ------- " << std::endl;
	   for( si = _data.begin(); si != _data.end(); si++) {

               if (si->first != "") 
	           std::cout << "[" << si->first << "]" << std::endl;

	       for (di = si->second.begin(); di != si->second.end(); di++) {
		   std::cout << di->first  << "=" << di->second << std::endl;
	       }

	   }
	   std::cout << "# ------ config dump end ------ " << std::endl;
    } catch (...) {
        std::cout << "exception in dump!" << std::endl;
    }
}

void
WimpyConfigParser::add_section(std::string sectionname) {
   std::map<std::string, std::string> np;
   _data.insert(std::make_pair(sectionname, np));
}

std::string 
WimpyConfigParser:: get(std::string sectionname, std::string optionname) {
   // need a section iterator and a data iterator..
   std::map<std::string, std::map<std::string, std::string> >::iterator si;
   std::map<std::string, std::string> ::iterator di;
   si = _data.find(sectionname);
   if (si != _data.end()) {
       di = si->second.find(optionname);
       if (di != si->second.end()) {
           return di->second;
       }
   }
   return std::string("");
}

void
WimpyConfigParser::set(std::string sectionname, std::string optionname, std::string value) {
   // need a section iterator 
   std::map<std::string, std::map<std::string, std::string> >::iterator si;
   si = _data.find(sectionname);
   if (si != _data.end()) {
       si->second.insert( make_pair(optionname, value));
   } else {
       std::cerr << "did not find section: " << sectionname << std::endl;
   }
}

bool 
WimpyConfigParser::has_section(std::string sectionname) {
   std::map<std::string, std::map<std::string, std::string> >::iterator si;
   si = _data.find(sectionname);
   return si != _data.end();
}

bool 
WimpyConfigParser::has_option(std::string sectionname, std::string optionname) {
   // need a section iterator and a data iterator..
   std::map<std::string, std::map<std::string, std::string> >::iterator si;
   std::map<std::string, std::string> ::iterator di;
   si = _data.find(sectionname);
   if (si != _data.end()) {
       di = si->second.find(optionname);
       return di != si->second.end();
   }
   return false;
}

}

#ifdef UNITTEST

#include <iostream>
extern "C" int exit(int);

using namespace ifdh_util_ns;

int
main() {
   WimpyConfigParser cp;
   cp.read("test.ini");
   std::cout << "global have x?" << cp.has_option("global", "x") << std::endl;
   std::cout << "global have y?" << cp.has_option("global", "y") << std::endl;
   std::cout << "global's y?" << cp.get("global", "y") << std::endl;
   std::cout << "test have x?" << cp.has_option("test", "x") << std::endl;
   std::cout << "test have y?" << cp.has_option("test", "y") << std::endl;
   std::cout << "test's x?" << cp.get("test", "x") << std::endl;
   cp.dump();
}
#endif
