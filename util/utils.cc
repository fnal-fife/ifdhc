#include "utils.h"
#include <errno.h>
#include <string.h>
#include <sstream>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <grp.h>
#include <dirent.h>
#include "WimpyConfigParser.h"

namespace ifdh_util_ns {
//
// get experiment name, either from CONDOR_TMP setting, or
// from group-id...
//

bool has(std::string s1, std::string s2) {
   return s1.find(s2) != std::string::npos;
}

int 
host_matches(std::string hostglob) {
   // match trivial globs for now: *.foo.bar
   static char namebuf[512];
   if ( 0 == namebuf[0])
       gethostname(namebuf, 512);
   std::string hostname(namebuf);
   size_t ps = hostglob.find("*");
   hostglob = hostglob.substr(ps+1);
   if ( std::string::npos != hostname.find(hostglob)) {
       return 1;
   } else {
       return 0;
   }
}

std::string
default_token_file() {
    std::stringstream tokenfile;
    char *btfenv = getenv("BEARER_TOKEN_FILE");
    char *xdgrtenv = getenv("XDG_RUNTIME_DIR");

    if (btfenv) {
       tokenfile << btfenv;
    } else {
       if (xdgrtenv) {
           tokenfile << xdgrtenv;
       } else {
           tokenfile << "/run/user/" << getuid();
       }
       tokenfile << "/bt_u" << getuid();
    }
    return tokenfile.str().c_str();
}

std::string parent_dir(std::string path) {
   size_t pos = path.rfind('/');
   if (pos == path.length() - 1) {
       pos = path.rfind('/', pos - 1);
   }
   // root of filesystem fix, return / for parent of /tmp ,etc.
   if (0 == pos) { 
      pos = 1;
   }
   return path.substr(0, pos);
}

std::string mount_dir(std::string path) {
   if (path[0] != '/') {
       path.insert(0,"/");
   }
   size_t pos = path.find('/',1);
   size_t pos2 = path.find('/', pos);
   while (pos2 == pos + 1) {
        // it's a URL?
        pos = path.find('/', pos2);
        pos2 = path.find('/', pos);
   }
   if (pos2 != std::string::npos) {
      return path.substr(0, pos2);
   } else {
      return "/";
   }
}


int
flushdir(const char *dir){
    // according to legend this flushes NFS directory cachng...
    DIR *dp;
    dp = opendir(dir);
    if (dp) {
        closedir(dp);
    }
    return 1;
}

#define MAXEXPBUF 64
const
char *getexperiment() {
       
    static char expbuf[MAXEXPBUF];
    char *p1;
    char *penv = getenv("EXPERIMENT");
    char *senv = getenv("SAM_EXPERIMENT");
    gid_t gid = getgid();
 
    //std::cerr << "Entering getexperiment\n";

    if (penv) {
        return penv;
    }
    if (senv) {
        return senv;
    }
  
    // if we already looked it up, return it
    if (expbuf[0] != 0) {
       return expbuf;
    }
  
    penv = getenv("CONDOR_TMP");
    if (penv) {
         /* if CONDOR_TMP looks like one of ours, use it */
         if (0 == strncmp(penv, "/fife/local/scratch/uploads/",28) ) {
             p1 = strchr(penv+29, '/');
             if(p1) {
                 *p1 = 0;
                 strncpy(expbuf, penv+28, MAXEXPBUF-1);
                 *p1 = '/';
                 return expbuf;
             }
         }
    }
    // Lookup gid mappings in config file...
    // we get our own copy to not break layering... sigh.
    
    WimpyConfigParser cf;
    cf.getdefault( getenv("IFDHC_CONFIG_DIR"), getenv("IFDHC_DIR"), getenv("IFDHC_FQ_DIR"),0);

    std::vector<std::string> explist = cf.getlist("gid_exp","gidexplist");
    std::string gids("gids_");
    for( std::vector<std::string>::iterator vsiexp = explist.begin(); vsiexp != explist.end(); vsiexp++) {
        // std::cerr << "Trying group:" << *vsiexp << "list:" << gids+*vsiexp << "\n";
        std::vector<std::string> gidlist = cf.getlist("gid_exp",gids+*vsiexp);
        for( std::vector<std::string>::iterator vsigid = gidlist.begin(); vsigid != gidlist.end(); vsigid++) {
            // std::cerr << "Trying gid:" << *vsigid << "\n";
            if (gid == (unsigned)atoi(vsigid->c_str())) {
                strncpy(expbuf, vsiexp->c_str(), MAXEXPBUF-1);
                return expbuf;
            }
        }
    }
    // also have fallback
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
    case 6269:
    case 8975:
        return "seaquest";
    default:
       struct group *pg;
       pg = getgrgid(gid);
       if (pg && pg->gr_name && getenv("USER") && strcmp(pg->gr_name, getenv("USER"))) {
           // if we have a group id and it doesn't match our usename
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
   if (s.size() == 0 ) {  // empty string -> empty list.
      return res;
   }
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

std::string
basename(std::string s) {
   size_t pos, slen, sslen;
   slen = s.length();
   pos = s.rfind('/', slen-2)+1;
   sslen = slen - pos - (s[slen-1] == '/');

   return s.substr(pos, sslen);
}

std::string
dirname(std::string s) {
   size_t pos, slen;
   slen = s.length();
   pos = s.rfind('/', slen-2);
          
   if( pos == std::string::npos )
       return "";
   return s.substr(0,pos);
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
    printf("\nexperiment (again): %s\n", getexperiment());

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

