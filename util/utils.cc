#include "utils.h"
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

namespace ifdh_util_ns {
//
// get experiment name, either from CONDOR_TMP setting, or
// from group-id...
//
#define MAXEXPBUF 64
const
char *getexperiment() {
    static char expbuf[MAXEXPBUF];
    char *p1;
    char *penv = getenv("EXPERIMENT");
 
    if (penv) {
        return penv;
    }
  
    penv = getenv("CONDOR_TMP");
    if (penv) {
         /* if CONDOR_TMP looks like one of ours, use it */
         p1 = strchr(penv+1, '/');
         if (p1 && 0 == strncmp(p1, "/data/",6) ) {
             *p1 = 0;
             strncpy(expbuf, penv+1, MAXEXPBUF);
             *p1 = '/';
             return expbuf;
         }
    }
    switch((int)getgid()){
    case 9257:
    case 9258:
    case 9259:
    case 9260:
    case 9261:
    case 9262:	
    case 9937:
    case 9952:
       return "microboone";
    case 5314:
       return "auger";
    case 9101:
    case 9112:
    case 9195:
    case 9226:
    case 9270:
    case 9272:
    case 9914:
       return "mu2e";
    case 9113:
    case 9950:
       return "gm2";
    case 5111:
       return "minos";
    case 9209:
       return "minosplus";
    case 1004:  // nova at smu
    case 9553:
       return "nova";
    case 9253:
    case 9555:
       return "minerva";
    default:
       return "other";
    }
}
//
// utility, split a string into a list -- like perl/python split()

int
find_end(std::string s, char c, int pos, bool quotes ) {
    size_t possible_end;
    
    possible_end = pos - 1;
    // handle quoted strings -- as long as  we're at a quote, look for the
    // next one... on the n'th pass throug the loop, possible end should
    // be at the following string indexes
    // string:  ?"xxxx""xxxx""xxxx",
    // pass:    0     1     2     3
    // If there's no quotes, we just pick up and find the 
    // separator (i.e. the comma)
    if (quotes || s[pos] == '"') {
        possible_end = s.find('"', possible_end+2);
    }
    
    // we could have mismatched quotes... so ignore them and start over
    if (possible_end == std::string::npos) {
        possible_end = pos - 1;
    }
    
    return s.find(c, possible_end+1);
}

std::vector<std::string>
split(std::string s, char c, bool quotes ){
   size_t pos, p2;
   pos = 0;
   std::vector<std::string> res;
   while( std::string::npos != (p2 = find_end(s,c,pos,quotes)) ) {
	res.push_back(s.substr(pos, p2 - pos));
        pos = p2 + 1;
   }
   res.push_back(s.substr(pos));
   return res;
}

void
fixquotes(char *s, int debug) {
   char *p1 = s, *p2 = s;

   if (debug) {
       std::cout << "before: " << s << "\n";
   }
   if (*p1 == '"') {
      p1++;
      while(*p1) {
         if (*p1 == '"') {
            if (*(p1+1) == '"') {
               p1++;
            } else {
               *p2++ = 0;
               break;
            }
         }
         *p2++ = *p1++;
      }
   }
   if (debug) {
      std::cout << "after: " << s << "\n";
      std::cout.flush();
   }
}

// get all but the first item of a vector..
std::vector<std::string> 
vector_cdr(std::vector<std::string> &vec) {
    std::vector<std::string>::iterator it = vec.begin();
    it++;
    std::vector<std::string> res(it, vec.end());
    return res;
}

}

#ifdef UNITTEST
main() {
    std::string data1("This,is,a,\"Test,with,a\",quoted,string");
    int i;
    
    std::vector<std::string> res;

    res = split(data1,',',1);
    for( i = 0; i<res.size(); i++ ) {
         std::cout << "res[" << i << "]: " << res[i] << "\n"; 
    }
}
#endif

