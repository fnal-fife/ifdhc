#ifndef IFDH_UTIL_H
#define IFDH_UTIL_H
#include <string>
#include <vector>
#include <iostream>

namespace ifdh_util_ns {

int find_end(std::string s, char c, int pos, bool quotes = false);

std::string join( std::vector<std::string> list, char sep );
std::vector<std::string> split(std::string s, char c, bool quotes = false );
void fixquotes(char *s, int debug);

// get all but the first item of a vector..
std::vector<std::string> vector_cdr(std::vector<std::string> &vec);

const char *getexperiment();


}
using namespace ifdh_util_ns;
#endif  // IFDH_UTIL_H
