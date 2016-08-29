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
#include <string>
#include <vector>

namespace ifdh_util_ns {

class WimpyConfigParser {
     private:
         std::map<std::string, std::map<std::string, std::string> > _data;
     public:
         WimpyConfigParser();
         void read(std::string filename);
         void readfp(std::istream &s);
         void add_section(std::string sectionname);
         void rename_section(std::string sectionname1,std::string sectionname2, bool copyflag = false);
         void copy_section(std::string sectionname1,std::string sectionname2);
         std::string rawget(std::string sectionname, std::string optionname);
         std::string expand(std::string val);
         std::string get(std::string sectionname, std::string optionname);
         int getint(std::string sectionname, std::string optionname);
         std::vector<std::string> getlist(std::string sectionname, std::string optionname);
         void set(std::string sectionname, std::string optionname, std::string value);
         bool has_section(std::string sectionname);
         bool has_option(std::string sectionname, std::string optionname);
         void dump();
};

}
#endif
