#include "WimpyConfigParser.h"
#include <fstream>
#include <iostream>
#include <stdlib.h>
#include <stdexcept>
#include <unistd.h>
#include "utils.h"
 
namespace ifdh_util_ns {

WimpyConfigParser::WimpyConfigParser(int debug) : _debug(debug) { ; }

void
WimpyConfigParser::
getdefault( const char *ccffile, const char *ccffile1, const char *ccffile2, int debug) {
    _debug = debug;
    std::string cffile;
    if (ccffile) {
        cffile = std::string(ccffile); 
        _debug && std::cerr << "ifdh: getting config file from IFDHC_CONFIG_DIR\n";
    } else if ( (ccffile1) && (std::ifstream((std::string(ccffile1) + "/ifdh.cfg").c_str())) ) {
        cffile = std::string(ccffile1); 
        _debug && std::cerr << "ifdh: getting config file from IFDHC_DIR --  no IFDHC_CONFIG_DIR?!?\n";
    } else if ( (ccffile2) && (std::ifstream((std::string(ccffile2) + "/ifdh.cfg").c_str())) ) {
        cffile = std::string(ccffile2); 
        _debug && std::cerr << "ifdh: getting config file from IFDHC_FQ_DIR --  no IFDHC_CONFIG_DIR?!?\n";
    } else {
	throw( std::logic_error("no ifdhc config file environment variables found"));
    }
    _debug && std::cerr << "ifdh: using config file: "<< ccffile << "/ifdh.cfg\n";

    //_debug && std::cerr << "ifdh: getting config file " << cffile << "/ifdh.cfg\n";
    read(cffile + "/ifdh.cfg");
    std::vector<std::string> clist = getlist("general","conditionals");
    _debug && std::cerr << "checking conditionals:\n";
    for( size_t i = 0; i < clist.size(); i++ ) { 
	_debug && std::cerr << "conditional" << i << ": " << clist[i] << "\n";
        if (clist[i] == "") {
           continue;
        }
        std::string rtype;
        std::string tststr = get("conditional " + clist[i], "test");
        std::vector<std::string> renamevec = getlist("conditional " + clist[i], "rename_proto");
        _debug && std::cerr <<"Renamevec.size() is " << renamevec.size() <<  " \n";
        if (renamevec.size() > 1) {
           rtype = "protocol ";
        } else {
            _debug && std::cerr <<  " trying rename_loc\n";
           renamevec = getlist("conditional " + clist[i], "rename_loc");
           if (renamevec.size() > 1) {
               rtype = "location ";
           } else {
                _debug && std::cerr <<  " trying rename_rot\n";
               renamevec = getlist("conditional " + clist[i], "rename_rot");
               if (renamevec.size() > 1) {
                   rtype = "rotation ";
               }
           }
        }
        _debug && std::cerr <<  " renamevec: " << renamevec[0] << ", " << renamevec[1] << "\n";
        _debug && std::cerr <<  " rtype: " << rtype << "\n";
        if (tststr[0] == '-' && tststr[1] == 'x') {
            if (0 == access(tststr.substr(3).c_str(), X_OK)) {
                _debug && std::cerr << "test: " << tststr << " renaming: " << renamevec[0] << " <= " << renamevec[1] << "\n";
		rename_section(rtype + renamevec[0], rtype + renamevec[1]);
            }
            continue;
        }
        if (tststr[0] == '-' && tststr[1] == 'r') {
            if (0 == access(tststr.substr(3).c_str(), R_OK)) {
                _debug && std::cerr << "test: " << tststr << " renaming: " << renamevec[0] << " <= " << renamevec[1] << "\n";
		rename_section(rtype + renamevec[0], rtype + renamevec[1]);
            }
            continue;
        }
        if (tststr[0] == '-' && tststr[1] == 'H') {
           if (host_matches(tststr.substr(3))) {
                _debug && std::cerr << "test: " << tststr << " renaming: " << renamevec[0] << " <= " << renamevec[1] << "\n";
		rename_section(rtype + renamevec[0], rtype + renamevec[1]);
           }
           continue;
        }
    }
    // handle protocol aliases
    std::vector<std::string> plist = getlist("general","protocols");
    std::string av;
    for( size_t i = 0; i < plist.size(); i++ ) { 
        av =  get("protocol " + plist[i], "alias");
        if (av != "" ) {
             copy_section("protocol " + av, "protocol " + plist[i]);
        }
    }
}

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
       while (p != std::string::npos ) {
          if(line[p-1] == '\\') {
              // backslashed #, trim the backslash
              line = line.substr(0,p-1) + line.substr(p);
          } else {
              // it has a comment on the end, trim it
              line = line.substr(0,p);
          }
          p = line.find('#',p);
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

int
WimpyConfigParser::size() {
   return _data.size();
}
void
WimpyConfigParser::add_section(std::string sectionname) {
   std::map<std::string, std::string> np;
   _data.insert(std::make_pair(sectionname, np));
}

void
WimpyConfigParser::copy_section(std::string sectionname1, std::string sectionname2) {
    rename_section(sectionname1, sectionname2, true);
}

void
WimpyConfigParser::rename_section(std::string sectionname1, std::string sectionname2, bool copyflag) {
   std::map<std::string, std::map<std::string, std::string> >::iterator si1,si2;
   si1 = _data.find(sectionname1);
   si2 = _data.find(sectionname2);
   if (si1 == _data.end()) {
       // didnt find it..
       return;
   }
   if (si2 == _data.end()){
       _data.erase(si2);
   }
   _data[sectionname2] = si1->second;
   if (!copyflag) {
       _data.erase(si1);
   }
}

int
WimpyConfigParser::getint(std::string sectionname, std::string optionname) {
   return atoi(get(sectionname, optionname).c_str());
}

std::vector<std::string>
WimpyConfigParser::getlist(std::string sectionname, std::string optionname) {
    return split(get(sectionname, optionname),' ');
}

// string lookup with macro expansion...
std::string 
WimpyConfigParser::get(std::string sectionname, std::string optionname) {
   return expand(rawget(sectionname, optionname));
}

// with macro expansion...
std::string 
WimpyConfigParser::expand(std::string res) {
   std::map<std::string, std::map<std::string, std::string> >::iterator si;
   std::map<std::string, std::string> ::iterator di;

   si = _data.find("macros");
   if (si != _data.end()) {
       for(di = si->second.begin(); di != si->second.end(); di++) {
           size_t pos = res.find("%(" + di->first + ")s");
           while (pos != std::string::npos) {
               res.replace(pos, di->first.size()+4, di->second);
               pos = res.find("%(" + di->first + ")s");
           }
       }
   }
   return res;
}

// basic string lookup
std::string 
WimpyConfigParser::rawget(std::string sectionname, std::string optionname) {
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
//extern "C" int exit(int);

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

   std::cout << "renaming test as renametest..." << std::endl;
   cp.rename_section("test","renametest");
   std::cout << "test have x?" << cp.has_option("test", "x") << std::endl;
   std::cout << "test have y?" << cp.has_option("test", "y") << std::endl;
   std::cout << "renametest have x?" << cp.has_option("renametest", "x") << std::endl;
   std::cout << "renametest have y?" << cp.has_option("renametest", "y") << std::endl;
   cp.dump();

   WimpyConfigParser ifdh_cfg;
   ifdh_cfg.read("../ifdh.cfg");
   ifdh_cfg.dump();
}
#endif
