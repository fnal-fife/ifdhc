#ifndef WIMPYCONFIGPARSER_H
#define WIMPYCONFIGPARSER_H

#ifndef _GLIBCXX_ISTREAM
#include <istream>
#endif
#ifndef _GLIBCXX_STRING
#include <string>
#endif
#ifndef _GLIBCXX_MAP
#include <map>
#endif

namespace ifdh_util_ns {

class WimpyConfigParser {
     private:
         std::map<std::string, std::map<std::string, std::string> > _data;
     public:
         WimpyConfigParser();
         void read(std::string filename);
         void readfp(std::istream &s);
         void add_section(std::string sectionname);
         std::string get(std::string sectionname, std::string optionname);
         void set(std::string sectionname, std::string optionname, std::string value);
         bool has_section(std::string sectionname);
         bool has_option(std::string sectionname, std::string optionname);
         void dump();
};

}
#endif
