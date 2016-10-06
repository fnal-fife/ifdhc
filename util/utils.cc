#include "utils.h"
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <grp.h>

namespace ifdh_util_ns {
//
// get experiment name, either from CONDOR_TMP setting, or
// from group-id...
//

bool has(std::string s1, std::string s2) {
   return s1.find(s2) != std::string::npos;
}

#define MAXEXPBUF 64
const
char *getexperiment() {
    static char expbuf[MAXEXPBUF];
    char *p1;
    char *penv = getenv("EXPERIMENT");
    char *senv = getenv("SAM_EXPERIMENT");
    gid_t gid = getgid();
 
    if (penv) {
        return penv;
    }
    if (senv) {
        return senv;
    }
  
  
    penv = getenv("CONDOR_TMP");
    if (penv) {
         /* if CONDOR_TMP looks like one of ours, use it */
         if (0 == strncmp(penv, "/fife/local/scratch/uploads/",28) ) {
             p1 = strchr(penv+29, '/');
             if(p1) {
                 *p1 = 0;
                 strncpy(expbuf, penv+28, MAXEXPBUF);
                 *p1 = '/';
                 return expbuf;
             }
         }
    }
    switch((int)gid){
    case 9257:
    case 9258:
    case 9259:
    case 9260:
    case 9261:
    case 9262:	
    case 9937:
    case 9952:
       return "uboone";
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
    case 9167:
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
    case 9157:
       return "lsst";
    case 9010:
        return "dune";
    case 9660:
        return "lbne"; //later fall through to...
    case 9620:
        return "des";
    case 9469:
        return "larp";
    case 9467:
        return "annie";
    case 9471:
        return "next";
    case 9985:
        return "darkside";
    default:
       struct group *pg;
       pg = getgrgid(gid);
       if (pg && pg->gr_name) {
           return pg->gr_name;
       } else {
           // we *really * don't know who they are...
           return "fermilab";
       }
    }
}

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
    if (quotes && s[pos] == '"') {
        possible_end = s.find('"', possible_end+2);
    }
    
    // we could have mismatched quotes... so ignore them and start over
    if (possible_end == std::string::npos) {
        possible_end = pos - 1;
    }
    
    return s.find(c, possible_end+1);
}

std::string
join( std::vector<std::string> list, char sep ) {
    std::string res;
    size_t lsize = list.size();
    size_t rsize = lsize;
    for (size_t i = 0; i < lsize; i++ ) {
         rsize += list[i].size();
    }
    res.reserve(rsize);
    for (size_t i = 0; i < lsize; i++ ) {
         res += list[i];
         if (i < lsize - 1) {
             res += sep;
         }
    }
   return res;
}

//
// utility, split a string into a list -- like perl/python split()
//
std::vector<std::string>
split(std::string s, char c, bool quotes , bool runs){
   size_t pos, p2;
   pos = 0;
   std::vector<std::string> res;
   while( std::string::npos != (p2 = find_end(s,c,pos,quotes)) ) {
	res.push_back(s.substr(pos, p2 - pos));
        pos = p2 + 1;
        if (runs) {
            // eat whole run of separators
            while ( pos < s.size() && s[pos] == c ){
                 pos = pos + 1;
            }
        }
   }
   res.push_back(s.substr(pos));
   return res;
}

void
fixquotes(char *s, int debug) {
   char *p1 = s, *p2 = s;

   if (debug) {
       std::cerr << "before: " << s << "\n";
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
      std::cerr << "after: " << s << "\n";
      std::cerr.flush();
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
#include <stdio.h>
int
main() {
    std::string data1("This,is,a,\"Test,with,a\",quoted,string");
    std::string data2("This   is      a   columned    string" );
    
    std::vector<std::string> res;

    printf("\nexperiment: %s\n", getexperiment());

    std::cout << "test data1 no quotes\n";
    res = split(data1,',',0,0);
    for( size_t i = 0; i<res.size(); i++ ) {
         std::cout << "res[" << i << "]: " << res[i] << "\n"; 
    }
    std::cout << "test data1..\n";
    res = split(data1,',',1,0);
    for( size_t i = 0; i<res.size(); i++ ) {
         std::cout << "res[" << i << "]: " << res[i] << "\n"; 
    }
    std::cout << "test data2..\n";
    res = split(data2,' ',0,1);
    for( size_t i = 0; i<res.size(); i++ ) {
         std::cout << "res[" << i << "]: " << res[i] << "\n"; 
    }

}
#endif

