
#ifndef WIMPYCONFIGPARSER_H
#include <WimpyConfigParser.h>
#endif
#include <ifstream>

namespace ifdh_util_ns {

void
WimpyConfigParser::read(std::string filename) {
   std::ifstream s(filename);
   readfp(s);
   s.close();
}


void
WimpyConfigParser::readfp(std::istream s) {
    std::string cur_section("");
    self.add_section(cur_section);
    bool have_partial = false;
    while( !s.eof() && !s.fail()) {
       getline(s, line);
       p = line.find_first_not_of(" \t");
       if (line[p] == '#' || line[p] == ';' ) {
          // it is a comment line
          continue;
       }
       if (line[0] == '[') {
          if (havepartial) {
              set(cur_section, cur_key, cur_val);
              cur_val = cur_key = "";
          }
          // section header
          p = line.find(']');
          cur_section = line.substr(1,p);
          self.add_section(cur_section);
       } else if (line[0] == ' ') {
          //continued line
          cur_val += line;
       } else  {
          //should be name = value
          if (havepartial) {
              set(cur_section, cur_key, cur_val);
              cur_val = cur_key = "";
          }
          p = line.find('=');
          cur_key = line.substr(0,p);
          p = line.find_first_not_of(" \t", p+1);
          cur_val = line.substr(p);
       }
    }
    if (havepartial) {
	set(cur_section, cur_key, cur_val);
    }
    return;
}

void
WimpyConfigParser::add_section(std::string sectionname) {
   map<std::string, std::string> np;
   _data.insert(make_pair<sectionname, np>);
}

std::string 
WimpyConfigParser:: get(std::string sectionname, std::string optionname) {
   // need a section iterator and a data iterator..
   std::map<std::string, std::map<std::string, std::string> >::iterator si;
   std::map<std::string, std::string> ::iterator di;
   si = _data.find(sectionname);
   if (si != data.end()) {
       di = si.second.find(optionname);
       if (di != si.second.end()) {
           return di->second;
       }
   }
   return "";
}

std::string 
WimpyConfigParser::set(std::string sectionname, std::string optionname, std::string value) {
   // need a section iterator 
   std::map<std::string, std::map<std::string, std::string> >::iterator si;
   si = _data.find(sectionname);
   if (si != data.end()) {
       si.second.insert( make_pair(optionname, value));
   }
}

bool 
WimpyConfigParser::has_section(std::string sectionname) {
   std::map<std::string, std::map<std::string, std::string> >::iterator si;
   si = _data.find(sectionname);
   return si != data.end();
}

bool 
WimpyConfigParser::has_option(std::string sectionname, std::string optionname) {
   // need a section iterator and a data iterator..
   std::map<std::string, std::map<std::string, std::string> >::iterator si;
   std::map<std::string, std::string> ::iterator di;
   si = _data.find(sectionname);
   if (si != data.end()) {
       di = si.second.find(optionname);
       return di != si.second.end();
   }
}
#endif
