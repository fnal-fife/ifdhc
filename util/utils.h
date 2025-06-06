#ifndef IFDH_UTIL_H
#define IFDH_UTIL_H
#include <string>
#include <vector>
#include <iostream>

namespace ifdh_util_ns {

std::string default_token_file();

int find_end(std::string s, char c, int pos, bool quotes = false);

bool has(std::string, std::string);
std::string join( std::vector<std::string> list, char sep );
std::vector<std::string> split(std::string s, char c, bool quotes = false, bool runs = false );
void fixquotes(char *s, int debug);

// get all but the first item of a vector..
std::vector<std::string> vector_cdr(std::vector<std::string> &vec);
const char *getexperiment();
int flushdir(const char *dir);
std::string parent_dir(std::string path);
std::string mount_dir(std::string path);
extern int host_matches(std::string glob);
extern std::string basename(std::string s);
extern std::string dirname(std::string s);

}
using namespace ifdh_util_ns;
#endif  // IFDH_UTIL_H
