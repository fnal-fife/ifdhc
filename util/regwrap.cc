#include <iostream>
#include "regwrap.h"

// classes to hide regex ugliness

namespace regex {

regmatch::regmatch(int n, std::string &s) :  _nslots(n), _against(s){
   _data = new regmatch_t[n+2];
   memset(_data, 0,  sizeof(regmatch_t) * (_nslots + 2));
   //std::cout << "in regmatch constructor with nslots =" << _nslots << "and against of: " << _against << "\n";
}

regmatch::regmatch(const regmatch &r) :   _nslots(r._nslots), _against(r._against){
   _data = new regmatch_t[r._nslots];
   memcpy(_data, r._data, sizeof(regmatch_t) * (r._nslots + 2));
   //std::cout << "in regmatch constructor with nslots =" << _nslots << "and against of: " << _against << "\n";
}

regmatch::~regmatch() {
   delete _data;
}

regmatch_t *
regmatch::data() {
   return _data;
}

void 
regmatch::set_matched(int n) {
   _matched = n;
   //std::cout << "setting matched to: " << n <<"\n";
   //std::cout << "have " << _nslots << "slots...\n";
}

std::string 
regmatch::operator[] ( int n ) {
    //std::cout << "in regmatch::operator [], data["<< n  <<"] is : " << _data[n].rm_so << ".." << _data[n].rm_eo << "\n";
    //std::cout << " and against is: " << _against << "\n";
    return _against.substr(_data[n].rm_so, _data[n].rm_eo - _data[n].rm_so);
}

regmatch::operator int() {
   return _matched == 0;
}

regexp::regexp(std::string re) {
   int r1;
   r1= regcomp( &_re, re.c_str(), REG_EXTENDED);
   if (r1 != 0) {
       static char buffer[1024];
       std::cout << "regcomp returns: " << r1 << "\n";
       std::cout << regerror(r1, &_re, buffer, 1024) << "\n";
   }
}

regexp::~regexp() {
   regfree(&_re);
}

regmatch 
regexp::operator () ( std::string &against) {
    regmatch result(_re.re_nsub, against);
    result.set_matched( regexec(&_re, against.c_str(), _re.re_nsub+1, result.data(), 0));
    return result;
}

}

#ifdef UNITTEST

int
main() {
    std::string s("aabbccc");
    std::string s2("aaaccc");
    regexp r("(a*)(b*)(c*)");

    regmatch m(r(s));
    regmatch m2(r(s2));

    if (m) {
       std::cout << "matched: got 0:" <<  m[0] << " 1: " << m[1] << " 2: " << m[2] << " 3: " << m[3] << "\n";
    } else {
       std::cout << "ouch!\n";
    }
    if (m2) {
       std::cout << "matched: got 0:" <<  m2[0] << " 1: " << m2[1] << " 2: " << m2[2] << " 3: " << m2[3] << "\n";
    } else {
       std::cout << "ouch!\n";
    }
}

#endif
