
#include <stdlib.h>
#include <fcntl.h>
#include "ifdh.h"
#include "utils.h"
#include "../util/WimpyConfigParser.h"
#include <iostream>
#include <string>
#include <cstring>
#include <sstream>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <../numsg/numsg.h>
#include <stdarg.h>
#include <string.h>
#include <memory>
#include <algorithm>

#include <stdlib.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/param.h>
#ifdef __APPLE__
#include <sys/vnode.h>
#include <sys/mount.h>
#define NFS_SUPER_MAGIC VT_NFS
#include <libgen.h>
#else
#include <sys/vfs.h> 
#include <linux/nfs_fs.h>
#endif
#include <ifaddrs.h>
#include <../util/regwrap.h>
#include <stdexcept>

using namespace std;

namespace ifdh_ns {


bool has(std::string s1, std::string s2) {
   return s1.find(s2) != std::string::npos;
}

struct IFile {
     std::string location;
     std::string path;
     std::string proto;
};

struct CpPair {
     IFile src, dst;
     std::string proto, proto2;

     CpPair() {;};
     CpPair( IFile s, IFile d, std::string p ) : src(s), dst(d), proto(p) {;};
};


IFile
lookup_loc(std::string url, WimpyConfigParser &_config) {
    IFile res;
    std::vector<std::string>  prefixes = split(_config.rawget("general","prefixes"),' ');
    ifdh::_debug && cerr << "lookup_loc " << url << "\n";
    for (std::vector<std::string>::iterator pp = prefixes.begin();  pp != prefixes.end(); pp++) {
        std::string s = *pp;
        if ( s == "" || s == " " ||   s == "\t" ) {
           // formatting artifact, skip it..
           continue;
        }
        std::string exps = _config.expand(s);
        ifdh::_debug && cerr << "checking prefix " << s << " -> " << exps << "\n";
        regexp pre("^"+exps);
        regmatch m(pre(url));
        if (m) {
            std::string lookup = "prefix " + s;
            std::string location = _config.get(lookup, "location");
            int slashstrip = _config.getint(lookup, "slashstrip");
            res.location = location;
            res.path = url;
            size_t pproto = url.find("://");
            if (pproto != std::string::npos && pproto < 11) {
                res.proto = url.substr(0,pproto+1);
            } else {
                res.proto = "";
            }

            ifdh::_debug && cerr << "trimming " << slashstrip << " slashes from " << url << "\n";

            for( int i = 0 ; i < slashstrip; i++ ) {
               size_t sp = res.path.find('/');
               if (sp != string::npos)
                   res.path.replace(0,sp+1, "");
            }

            ifdh::_debug && cerr << "trimmed to  "  << res.path << "\n";
            return res;
        }
    }
    throw( std::logic_error("Path " + res.path +  " did not match in configuration"));
}


// one special case here, when you're copying to a local 
std::string
locpath(IFile loc, std::string proto, WimpyConfigParser &_config) {
   std::string pre;
   if (loc.location == "local_fs") { // XXX should be a flag
      if (_config.getint("protocol " + proto, "strip_file_prefix")) {
          pre = "";
      } else {
          pre = "file:///";
      }
   } else {
      pre  = _config.get("location " + loc.location, "prefix_"+proto.substr(0,proto.size()-1));
   }
   return pre + loc.path;
}



struct stat *
cache_stat(std::string s) {
   int res;
   static struct stat sbuf;
   static string last_s;
   if (last_s == s) {
       return &sbuf;
   }
   res = stat(s.c_str(), &sbuf);
   if (res != 0) {
       return 0;
   }
   last_s = s;
   return  &sbuf;
}

bool
have_stage_subdirs(std::string uri, ifdh *ih) {
   int count = 0;
   vector<string> list = ih->ls(uri,1,"");

   for (size_t i = 0; i < list.size(); i++) {
       if(list[i].find("ifdh_stage/queue/") != string::npos ) count++;
       if(list[i].find("ifdh_stage/lock/") != string::npos )  count++;
       if(list[i].find("ifdh_stage/data/") != string::npos )  count++;
   }
   return count == 3;
}

extern std::string datadir();

// file io

class cpn_lock {

private:

    int _heartbeat_pid;
    int _locked;

public:

    int
    locked() { return _locked; }

    void
    lock() {
	char buf[512];
	FILE *pf;
        int onsite = 0;
        int parent_pid;
        int status;
        struct ifaddrs *ifap, *ifscan;

        // check if we're onsite -- don't get locks if not 
        // Just check for 131.225.* or 2620:6a:0:*
        
        if (0 == getifaddrs(&ifap)) {
           ifdh::_debug && cerr << "Got addrs...\n";
           for( ifscan = ifap; ifscan ; ifscan = ifscan->ifa_next) {
               if( ifscan->ifa_addr->sa_family == AF_INET && ifscan->ifa_addr->sa_data[2] == (char)131  && ifscan->ifa_addr->sa_data[3] == (char)225) {
                   ifdh::_debug && cerr << "Saw ipv4 for Fermilab: onsite\n";
                   onsite = 1;
               }
               if( ifscan->ifa_addr->sa_family == AF_INET6 && ifscan->ifa_addr->sa_data[6] == 0x26  && ifscan->ifa_addr->sa_data[7] == 0x20 && ifscan->ifa_addr->sa_data[8] == 0x0 && ifscan->ifa_addr->sa_data[9] == 0x6a && ifscan->ifa_addr->sa_data[10] == 0x0 && ifscan->ifa_addr->sa_data[11] == 0x0 ) {
                   ifdh::_debug && cerr << "Saw ipv6 for Fermilab: onsite\n";
                   onsite = 1;
               }
           }
           freeifaddrs(ifap);
           if (!onsite) {
              return;
           }
        }

        if (!getenv("CPN_DIR") || 0 != access(getenv("CPN_DIR"),R_OK)) {
            return;
        }

	// call lock, skip to last line 
	pf = popen("exec $CPN_DIR/bin/lock","r");
	while (!feof(pf) && !ferror(pf)) {
            if (fgets(buf, 512, pf)) {
                fputs(buf,stderr);
                fflush(stderr);
            }
	}
        if (ferror(pf)) {
	    pclose(pf);
	    throw( std::logic_error("Could not get CPN lock: error reading lock output"));
        }
	status = pclose(pf);
        if (WIFSIGNALED(status)) throw( std::logic_error("signalled while  getting lock"));

	// pick lockfile name out of last line
	std::string lockfilename(buf);
	size_t pos = lockfilename.rfind(' ');
	lockfilename = lockfilename.substr(pos+1);

        // trim newline
	lockfilename = lockfilename.substr(0,lockfilename.length()-1);

	// kick off a backround thread to update the
	// lock file every minute that we're still running

	if ( lockfilename[0] != '/' || 0 != access(lockfilename.c_str(),W_OK)) {
	    throw( std::logic_error("Could not get CPN lock."));
	}
     
        _locked = 1;
	_heartbeat_pid = fork();
	if (_heartbeat_pid == 0) {
	    parent_pid = getppid();
	    while( 0 == kill(parent_pid, 0) ) {
                // touch our lockfile
		if ( 0 != utimes(lockfilename.c_str(), NULL)) {
                    perror("Lockfile touch failed");
                    exit(1);
                }
		sleep(60);
	    }
	    exit(0);
	}
    }

    void
    free() {
        int res, res2;
        if (!getenv("CPN_DIR") || 0 != access(getenv("CPN_DIR"),R_OK)) {
            return;
        }
        if (_heartbeat_pid < 0) {
            return;
        }
        kill(_heartbeat_pid, 9);
        waitpid(_heartbeat_pid, &res, 0);
        res2 = system("exec $CPN_DIR/bin/lock free >&2");
        _heartbeat_pid = -1;
        _locked = 0;
        if (!((WIFSIGNALED(res) && 9 == WTERMSIG(res)) || (WIFEXITED(res) &&WEXITSTATUS(res)==0))) {
            stringstream basemessage;
            basemessage <<"lock touch process exited code " << res << " signalled: " << WIFSIGNALED(res) << " signal: " << WTERMSIG(res);
            throw( std::logic_error(basemessage.str()));
        }
        if (WIFSIGNALED(res2)) {
            throw( std::logic_error("signalled while doing srmping"));
        }
    }

    cpn_lock() : _heartbeat_pid(-1), _locked(0) { ; }
 
    ~cpn_lock()  {

       if (_heartbeat_pid != -1) {
           free();
       }
    }
};

std::vector<std::string> expandfile( std::string fname, std::vector<std::string> oldargs, unsigned int oldcount, unsigned int oldcount2) {
  std::vector<std::string> res;
  std::string line;
  size_t pos, pos2;

  bool first = true;

  for(size_t i = 0; i < oldcount2; i++) {
       res.push_back(oldargs[i]);
  }
  fstream listf(fname.c_str(), fstream::in);
  getline(listf, line);
  while( !listf.eof() && !listf.fail()) {
      ifdh::_debug && std::cerr << "expand: line: " << line << "\n";
      if( !first ) {
  	 res.push_back( ";" );
      }
      first = false;
      // trim trailing whitespace
      while ( ' ' == line[line.length()-1] || '\t' == line[line.length()-1] || '\r' == line[line.length()-1] ) {
          line = line.substr(0,line.length()-1);
      }
      pos = line.find_first_of(" \t");
      if (pos != string::npos) {
         pos2 = pos;
         while ( ' ' == line[pos2+1] || '\t' == line[pos2+1]) {
             pos2++;
         }
         res.push_back( line.substr(0,pos) );
         res.push_back( line.substr(pos2+1) );
         ifdh::_debug && std::cerr << "expand: first: " << line.substr(0,pos) << " second: " << line.substr(pos2+1) << "\n";
      }
      getline(listf, line);
  }
  if ( !listf.eof() ) {
      std::string basemessage("error reading list of files file: ");
      throw( std::logic_error(basemessage += fname));
  }
  if (oldcount < oldargs.size()) {
     res.push_back(";");
     while (oldcount < oldargs.size()) {
        res.push_back(oldargs[oldcount++]);
     }
  }
 
  return res;
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
   ifdh::_debug && cerr << "parent of " << path << " is " << path.substr(0, pos ) << endl;
   return path.substr(0, pos);
}

//
// figure the destination filename for a source file
// and a destination directory.
//

std::string dest_file( std::string src_file, std::string dest_dir) {
    size_t pos;
    pos = src_file.rfind('/');
    if (pos == std::string::npos) {
        pos = 0;
    } else {
        pos = pos + 1;
    }
    return dest_dir + "/" + src_file.substr(pos);
}

std::vector<std::string> 
ifdh::build_stage_list(std::vector<std::string> args, int curarg, char *stage_via) {
   std::vector<std::string>  res;
   std::string stagefile(datadir());
   std::string ustring;
   std::string stage_location;

   // unique string for this stage out
   ustring = unique_string();

   // if we are told how to stage, use it, fall back to OSG_SITE_WRITE,
   //  or worst case, pnfs scratch
   std::string base_uri(stage_via);
   if (base_uri[0] == '$') {
       base_uri = base_uri.substr(1);
       base_uri = getenv(base_uri.c_str());
   }
   base_uri = (base_uri + "/" + getexperiment());
   stagefile += "/stage";
   stagefile += ustring;

   // double slashes past the first srm:// break srmls
   // so clean any other double slashe out.
   size_t dspos = base_uri.rfind("//");
   while (dspos != string::npos && dspos > 7) {
      base_uri = base_uri.substr(0,dspos) + base_uri.substr(dspos+1);
      dspos = base_uri.rfind("//");
   }

   // make sure directory hierarchy is there..
   if (!have_stage_subdirs(base_uri + "/ifdh_stage", this)) {
       try{ mkdir( base_uri , "");               } catch (...){;};
       try{ mkdir( base_uri + "/ifdh_stage", ""); } catch(...) {;};
       try{ mkdir( base_uri + "/ifdh_stage/queue" , "");}  catch(...){;}
       try{ mkdir( base_uri + "/ifdh_stage/lock", ""); } catch(...){;}
       try{ mkdir( base_uri + "/ifdh_stage/data", ""); } catch(...){;}
   }

   mkdir( base_uri + "/ifdh_stage/data/" + ustring, "");

   // open our stageout queue file/copy back instructions
   fstream stageout(stagefile.c_str(), fstream::out);

   bool staging_out = false;

   for( std::vector<std::string>::size_type i = curarg; i < args.size(); i+=3 ) {

     if (0 == access(args[i].c_str(), R_OK) && 0 != access(args[i+1].c_str(), R_OK)) {
       staging_out = true;
       // we're going to keep this in our stage queue area
       stage_location = base_uri + "/ifdh_stage/data/" + ustring + "/" +  basename((char *) args[i].c_str());

       // we copy to the stage location
       res.push_back(args[i]);
       res.push_back(stage_location);
       res.push_back(";");

       // the stage item out is from there to the final destination
       stageout << stage_location << ' ' << args[i+1]  << '\n';
     } else {
       res.push_back(args[i]);
       res.push_back(args[i+1]);
       res.push_back(";");
     }
   }

   // copy our queue file in last, it means the others are ready to copy
   if ( staging_out ) {
      string stagebase = stagefile.substr(stagefile.rfind('/')+1);
      res.push_back( stagefile );
      res.push_back( base_uri + "/ifdh_stage/queue/" + stagebase );
   } else {
      res.pop_back();
   }

   return res;
}

bool 
check_grid_credentials() {
    static char buf[512];
    FILE *pf = popen("voms-proxy-info -all 2>/dev/null", "r");
    bool found = false;
    std::string experiment(getexperiment());
    std::string path;
    
    ifdh::_debug && std::cerr << "check_grid_credentials:\n";

    if (experiment == "samdev")  // use fermilab for fake samdev expt
        experiment = "fermilab";

    while(fgets(buf,512,pf)) {
	 std::string s(buf);

         if ( 0 == s.find("path ")) {
             path = s.substr(s.find(':') + 2);
             path = path.substr(0,path.size()-1);
             ifdh::_debug && std::cerr << "saw path" << path << "\n";
         }
	 if ( 0 == s.find("attribute ") && std::string::npos != s.find("Role=") && std::string::npos == s.find("Role=NULL")) { 
	     found = true;
             if (std::string::npos ==  s.find(experiment)) {
                 std::cerr << "Notice: Expected a certificate for " << experiment << " but found " << s << endl;
             }
             ifdh::_debug && std::cerr << "found: " << buf << endl;
	 }
	 // if it is expired, its like we don't have it...
	 if ( 0 == s.find("timeleft  : 0:00:00")) { 
	     found = false;
             ifdh::_debug && std::cerr << "..but its expired\n " ;
	 }
    }
    fclose(pf);

    if (found and 0 == getenv("X509_USER_PROXY")) {
        ifdh::_debug && std::cerr << "setting X509_USER_PROXY to " << path << "\n";
        setenv("X509_USER_PROXY", path.c_str(),1);
        // for xrdcp...
        setenv("XrdSecGSIUSERCERT", path.c_str(),1);
        setenv("XrdSecGSIUSERKEY", path.c_str(),1);
    }
    return found;
}

//
// check for kerberos credentials --  use klist -s
// 
bool
have_kerberos_creds() {
    return 0 == system("klist -5 -s || klist -s");
}


//
// you call this if you need to do any kind of SRM or Gridftp
// transfer, and if you're running interactive it grabs a 
// proxy for you if you don't have one
//
//

void
get_grid_credentials_if_needed() {
    std::string cmd;
    std::string experiment(getexperiment());
    std::stringstream proxyfileenv;
    int res;

    ifdh::_debug && std::cerr << "Checking for proxy cert..."<< endl;

    if( getenv("IFDH_NO_PROXY"))
        return;
   
    if (!check_grid_credentials() && have_kerberos_creds()) {
        // if we don't have credentials, try our standard copy cache file
        proxyfileenv << datadir() << "/x509up_cp" << getuid();
	ifdh::_debug && std::cerr << "no credentials, trying " << proxyfileenv.str() << endl;
        setenv("X509_USER_PROXY", proxyfileenv.str().c_str(),1);
        // for xrdcp...
        setenv("XrdSecGSIUSERCERT", proxyfileenv.str().c_str(),1);
        setenv("XrdSecGSIUSERKEY", proxyfileenv.str().c_str(),1);
        ifdh::_debug && std::cerr << "Now X509_USER_PROXY is: " << getenv("X509_USER_PROXY")<< endl;
    }

    if (!check_grid_credentials() && have_kerberos_creds() ) {
        // if we still don't have credentials, try to get some from kx509
	ifdh::_debug && std::cerr << "trying to kx509/voms-proxy-init...\n " ;

        ifdh::_debug && system("echo X509_USER_PROXY is $X509_USER_PROXY >&2");

	cmd = "kx509 ";
        if (ifdh::_debug) {
            cmd += " >&2 ";
        } else {
            cmd += " >/dev/null 2>&1 ";
        }
        cmd += "&& voms-proxy-init -dont-verify-ac -rfc -noregen -debug -voms ";

        // table based vo mapping...
        std::string vo;
        int numrules = ifdh::_config.getint("experiment_vo","numrules");
        for(int i = 1; i <= numrules ; i++) {

            stringstream numbuf;
            numbuf << i;
            std::string match(ifdh::_config.get("experiment_vo", "match"+numbuf.str()));
            regexp exre(match);
            std::string repl(ifdh::_config.get("experiment_vo","repl"+numbuf.str()));
            regmatch m1(exre(experiment));

	    ifdh::_debug && std::cerr << "vo map: " << i << " of  " << numrules << "\n";
	    ifdh::_debug && std::cerr << "vo map: checking: /" << match << "/ against: " << experiment << "\n";

            if (m1) {
                vo = repl;
                while (has(vo,"$1")) {
                    vo = vo.replace(vo.find("$1"),2,m1[1]);
                }
                while (has(vo,"$2")) {
                    vo = vo.replace(vo.find("$2"),2,m1[2]);
                }
                while (has(vo,"$3")) {
                    vo = vo.replace(vo.find("$3"),2,m1[3]);
                }
                ifdh::_debug && std::cerr << "vo map: matched: /" << match << "/ got : " << vo << "\n";
                break;
            }
            
        }
        cmd += vo + "/Role=Analysis" ;

        if (ifdh::_debug) {
            cmd += " >&2";
        } else {
            cmd += " >/dev/null 2>&1";
        }

	ifdh::_debug && std::cerr << "running: " << cmd << endl;
	res = system(cmd.c_str());
        // try a second time if it failed...
        if (!WIFEXITED(res) || 0 != WEXITSTATUS(res)) {
           sleep(1);
	   res = system(cmd.c_str());
        }
        if (!WIFEXITED(res) ||  0 != WEXITSTATUS(res)) {
            std::cerr << "Error from voms-proxy-init... later things may fail\n";
        }
    }
}

std::string
ifdh::getProxy() {
   get_grid_credentials_if_needed();
   std::string res( getenv("X509_USER_PROXY"));
   return res;
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

char *
parse_ifdh_stage_via() {
   static char resultbuf[1024];
   char *fullvia = getenv("IFDH_STAGE_VIA");
   size_t start, loc1, loc2;

   if (!fullvia)
         return 0;
   std::string svia(fullvia);
   start = 0;
   if (std::string::npos != (loc1 = svia.find("=>",start))) {
      while (std::string::npos != (loc2 = svia.find(";;",start))) {
           if (host_matches(svia.substr(start, loc1 - start))) {
               strncpy(resultbuf,svia.substr( loc1+2,loc2 - loc1 - 2).c_str(),1024);
               if ( 0 != strlen(resultbuf) ) {
                  return resultbuf;
               } else {
                  return 0;
               }
           }
           start = loc2+2;
           loc1 = svia.find("=>",start);
      }
      return 0;
   } else {
      // no fancy stuff...
      return fullvia;
   }
}


string
get_pnfs_uri(std::string door_url = "http://fndca3a.fnal.gov:2288/info/doors", std::string door_proto = "gsiftp") {
    int state = 0;
    static vector<string> nodes;
    static string cached_node_proto;
    string line;
    static string cached_result;

    //if (cached_result != "")
    //    return cached_result;
    if (door_proto[door_proto.size()-1] == ':') {
        door_proto= door_proto.substr(0,door_proto.size()-1);
    }

    if (0 == nodes.size() || cached_node_proto != door_proto) {
        nodes.clear();
        ifdh::_debug && cerr << "looking for dcache " << door_proto << " doors..\n";
        WebAPI wa(door_url);
	while (!wa.data().eof() && !wa.data().fail()) {
	    getline(wa.data(), line);
            // ifdh::_debug && cerr << "got: " << line << "\n";
	    if (line.find("<door") != string::npos) {
	       state = 1;
	    }
	    if (line.find("</door") != string::npos) {
	       state = 0;
	    }
	    if (state == 1 && line.find("<metric name=\"family") && line.find(">" + door_proto + "<") != string::npos) {
	       state = 2;
	    }
	    if (state == 2 && line.find("<interfaces") != string::npos) {
	       state = 3;
	    }
	    if (state == 3 && line.find("</interfaces") != string::npos) {
	       state = 2;
	    }
	    if (state == 3 && line.find("metric name=\"FQDN") != string::npos ) {
	       size_t p1 = line.find(">");
	       size_t p2 = line.find("<");
	       p2 = line.find("<", p2+1);
               string node = line.substr(p1+1, p2-(p1+1));
               if (node == "fndca4a.fnal.gov") 
                   continue;
	       nodes.push_back(node);
               ifdh::_debug && cerr << "found dcache door: " << node << "\n";
	    }
	}
    }
    cached_node_proto = door_proto;
    if ( nodes.size() == 0) {
       // couldn't get nodes from website...
       // or didn't find a match..
       return "";
    }
    int32_t rn;
    rn = random();
    cached_result = door_proto + "://" + nodes[ rn % nodes.size() ] + "/pnfs/fnal.gov/usr/";
    return cached_result;
}

void
get_another_dcache_door( std::string &cmd, std::string door_regex, std::string door_url, std::string door_proto ) {
    size_t base;
    std::string repl;

    regexp door_re(door_regex);
    
    base = 0;

    while(1)  {
         std::string subcmd =  cmd.substr(base);

         regmatch doors(door_re(subcmd));

         if (!doors) {
             break;
         }
         repl = get_pnfs_uri(door_url, door_proto);

         if (repl == "") {
             return;
         }

         ifdh::_debug && cerr << "repl is " << repl << "\n";

         regmatch replmatch(door_re(repl));

         ifdh::_debug && cerr << "replmatch[0] is " << replmatch[0] << "\n";

         string trimrepl = repl.substr(replmatch.data()[0].rm_so, replmatch.data()[0].rm_eo - replmatch.data()[0].rm_so);
         ifdh::_debug && cerr << "doors[0] is " << doors[0] << "\n";

         ifdh::_debug && cerr << "replacing: " << cmd.substr(base + doors.data()[0].rm_so, doors.data()[0].rm_eo - doors.data()[0].rm_so) << "with " << trimrepl << "\n";
         cmd.replace(base + doors.data()[0].rm_so, doors.data()[0].rm_eo - doors.data()[0].rm_so, trimrepl);
         base = base + doors.data()[0].rm_eo;
    }
    
    ifdh::_debug && cerr << "finally, cmd is: "  << cmd << "\n";
}

bool
is_dzero_node_path( std::string path ) {
 // it could be a clued0 node, or it could be a d0srv node...
 // and the d0srv is either at the front, or has user@ on the
 // front of it...
 return path.find("D0:") == 0;
}

int
retry_system(const char *cmd_str, int error_expected, cpn_lock &locker,  std::string door_regex = "", std::string door_url="", std::string door_proto = "", int maxtries = -1) {
    int res = 1;
    int tries = 0;
    int delay;
    int dolock = locker.locked();
    std::string cmd_str_string;
    if (maxtries == -1) {
        if (0 != getenv("IFDH_CP_MAXRETRIES")) {
            maxtries = atoi(getenv("IFDH_CP_MAXRETRIES")) + 1;
        } else {
            maxtries = 8;
        }
    }


    while( res != 0 && tries < maxtries ) {
        if (door_regex != "" ) {
            regexp door_re(door_regex);

	    cmd_str_string = cmd_str;

	    // do door proto etc. here...
            // XXX someday there may be *two* dcache-ish instances
            // we are copying between, and we want to rotate both
            // doors, so we would need to call this twice, once with
            // the src door one, and once with the destination...
            // so we may need *two* door_re's etc...
	    if (0 != door_re(cmd_str_string)) {
	       get_another_dcache_door(cmd_str_string, door_regex, door_url, door_proto );   
               cmd_str = cmd_str_string.c_str();
	    }
        }
              
        res = system(cmd_str);
        if (WIFEXITED(res)) {
            res = WEXITSTATUS(res);
        } else {
            std::cerr << "program: " << cmd_str<< " died from signal " << WTERMSIG(res) << "-- exiting.\n";
            exit(-1);
        }
        if (res != 0 && error_expected) {
           return res;
        }
        if (res != 0 && tries < maxtries - 1) {
            if (dolock)
                locker.free();
            std::cerr << "program: " << cmd_str << "exited status " << res << "\n";
            delay =random() % (55 << tries);
            std::cerr << "delaying " << delay << " ...\n";
            sleep(delay);
            std::cerr << "retrying...\n";
            if (dolock)
                locker.lock();
        }
        tries++;
    }
    return res;
}


void
make_canonical( std::string &arg ) {

    static char getcwd_buf[MAXPATHLEN];
    static string cwd(getcwd(getcwd_buf, MAXPATHLEN));

    if (cwd[0] != '/') {
        throw( std::logic_error("unable to determine current working directory"));
    }


   if (arg[0] != ';' 
       && arg[0] != '-' 
       && arg[0] != '/' 
       && (arg.find(":/") == string::npos || arg.find(":/") > 10)
       && !is_dzero_node_path(arg)) {
       ifdh::_debug && std::cerr << "adding cwd to " << arg << endl;
       arg = cwd + "/" + arg;
   }
   // clean out doubled slashes...uless its the one in the ://
   // set a fence position after the :// if any...
   size_t fpos = arg.find("://");
   if (fpos == string::npos || fpos > 10) {
       fpos = 0;
   } else {
       fpos += 3;
   }
   size_t dspos = arg.rfind("//");
   while (dspos != string::npos && dspos >= fpos) {
      arg = arg.substr(0,dspos) + arg.substr(dspos+1);
      dspos = arg.rfind("//");
   }
}

std::vector<std::string>::size_type 
parse_opts( std::vector<std::string> &args, bool &dest_is_dir, bool &recursive, string &force, bool &no_zero_length, bool &intermed_file_flag ) {
    std::vector<std::string>::size_type curarg = 0;
    for( curarg = 0; curarg < args.size(); curarg++) {
        if (args[curarg][0] == '-') {
            if (args[curarg][1] == '-') {
                if (args[curarg][2] == 'f') {
                   force = args[curarg].substr(8);
                } else if (args[curarg][2] == 'n') {
                   no_zero_length = 1;
                } else if (args[curarg][2] == 'i') {
                   intermed_file_flag = 1;
                }
            } else if (args[curarg][1] == 'f') {
                args = expandfile(args[curarg+1], args, curarg+2, curarg);
                if (ifdh::_debug) {
                    std::cerr << "after expanding file: args are: ";
                    for(size_t j = 0; j < args.size(); j++) {
                       std::cerr << args[j] << " ";
                    }
                    std::cerr << "\n";
                }
                curarg = curarg - 1;
            } else if (args[curarg][1] == 'D') {
                dest_is_dir = 1;
            } else if (args[curarg][1] == 'r') {
                recursive = 1;
            }
        } else {
            break;
        }
    }
    return curarg;
}

IFile
get_intermed(bool intermed_file_flag) {
    IFile res;
    string l = datadir() + "/intermed";
    res.location = "local_fs";
    res.path = l;
    unlink(l.c_str());
    if (intermed_file_flag) {
       return res;
    } else {
       (void)mknod(l.c_str(), 0600 | S_IFIFO, 0);  // XXX error handling
       return res;
    }
}


std::string
srcpath(CpPair &cpp, WimpyConfigParser &_config) {
   struct stat sb;
   std::string res = locpath(cpp.src, cpp.proto, _config);
   if (_config.getint("protocol " + cpp.proto, "redir_pipe")) {
       if (0 == stat(res.c_str(), &sb)) {
          if (S_ISFIFO(sb.st_mode)) {
             res = "-<" + res;
          }
       }
   }
   return res;
}

std::string
dstpath(CpPair &cpp, WimpyConfigParser &_config) {
   struct stat sb;
   std::string res = locpath(cpp.dst, cpp.proto, _config);
   if (_config.getint("protocol " + cpp.proto, "redir_pipe")) {
       if (0 == stat(res.c_str(), &sb)) {
          if (S_ISFIFO(sb.st_mode)) {
             res =  "->" + res;
          }
       }
   } return res;
}

int
do_cp_bg(CpPair &cpp, WimpyConfigParser &_config, bool intermed_file_flag, bool recursive, cpn_lock &cpn);

int
do_cp(CpPair &cpp, WimpyConfigParser &_config, bool intermed_file_flag, bool recursive, cpn_lock &cpn) {
    int res1, res2, pid;
    if ( cpp.proto2 != "" ) {
        // deail with cross-protocol copies
	IFile iftmp = get_intermed(intermed_file_flag);
	CpPair cp1(cpp.src, iftmp, cpp.proto), cp2(iftmp, cpp.dst, cpp.proto2);
	if (intermed_file_flag) {
	   res1 = do_cp(cp1, _config, intermed_file_flag, recursive, cpn); 
           if (res1 != 0) {
               unlink(iftmp.path.c_str());
               return res1;
           }
	   res2 = do_cp(cp2, _config, intermed_file_flag, recursive, cpn);
	   unlink(iftmp.path.c_str());
	   return res2;
	} else {
	   pid = do_cp_bg(cp1, _config, intermed_file_flag, recursive, cpn);
           if (pid > 0) {
	       res2 = do_cp(cp2, _config, intermed_file_flag, recursive, cpn);
	       (void)waitpid(pid, &res1, 0);
               unlink(iftmp.path.c_str());
               return res2;
           } else {
               unlink(iftmp.path.c_str());
	       return pid;
           }
	}
    } else {
        std::string lookup("protocol " + cpp.proto);
        std::string extra_env;
        const char *extra_env_val;
        std::string cp_cmd;
        
        // command looks like:
        // cp_cmd=dd bs=512k %(extra)s if=%(src)s of=%(dst)s
	extra_env = _config.get(lookup, "extra_env");
        extra_env_val = getenv(extra_env.c_str());
        if (extra_env_val && strlen(extra_env_val) == 0 ) {
	    extra_env = _config.get(lookup, "extra_env_2");
            extra_env_val = getenv(extra_env.c_str());
        }
        if (_config.getint(lookup, "need_proxy") ) {
	    get_grid_credentials_if_needed();
        }
        if (recursive) {
	    cp_cmd = _config.get(lookup, "cp_r_cmd");
        } else {
	    cp_cmd = _config.get(lookup, "cp_cmd");
        }
        cp_cmd.replace(cp_cmd.find("%(src)s"), 7, srcpath(cpp, _config));
        cp_cmd.replace(cp_cmd.find("%(dst)s"), 7, dstpath(cpp, _config));
        if (! extra_env_val)
            extra_env_val = "";
        std::string seev(extra_env_val);

        cp_cmd.replace(cp_cmd.find("%(extra)s"), 9, seev);

        ifdh::_debug && cerr << "running: " << cp_cmd << "\n";

        std::string door_re;
        std::string door_uri;
        
        if ( (door_re = _config.get("location " + cpp.src.location, "door_repl_re")) != "") {
            door_uri = _config.get("location " + cpp.src.location, "lookup_door_uri");
        } else {
            door_re = _config.get("location " + cpp.dst.location, "door_repl_re");
            door_uri = _config.get("location " + cpp.dst.location, "lookup_door_uri");
        }
        
        return retry_system(cp_cmd.c_str(), 0, cpn, door_re, door_uri, cpp.proto );
    }
}

//
// do a copy in background, return the pid or -1
//
int
do_cp_bg(CpPair &cpp, WimpyConfigParser &_config, bool intermed_file_flag, bool recursive, cpn_lock &cpn) {
    ifdh::_debug && std::cerr << "do_cp_bg: starting...\n";
    int res2 = fork();
    if (res2 == 0) {
       ifdh::_debug && std::cerr << "do_cp_bg: in child, about to call do_cp\n";
       int res = do_cp(cpp, _config, intermed_file_flag, recursive, cpn);
       ifdh::_debug && std::cerr << "do_cp_bg: in child, to return "<< res << "\n";
       if (res != 0 ) {
           // unwedge other side if cp failed
           int f = open(cpp.dst.path.c_str(),O_WRONLY|O_NDELAY, 0700);
           if (f >= 0) {
               close(f);
           }
       }
       exit(res);
    } else {
       ifdh::_debug && std::cerr << "do_cp_bg: in parent, returning pid/err\n";
       return res2;
    }
}
void
pick_proto(CpPair &p, WimpyConfigParser & _config, std::string force) {
    // pick the first common protocol between the two..
    // XXX should this instead go through our overall protocol list,
    // and pick the first one in both lists? Or if we're consistent
    // in the config, it works anyway...
    
    if (force.length() == 0) {
       if (getenv("IFDH_FORCE")) {
          force += "--force=";
          force += getenv("IFDH_FORCE");
       }
    }

    std::string srcps = _config.get("location "+p.src.location,"protocols");
    std::string dstps = _config.get("location "+p.dst.location,"protocols");

    if (force != "") {
        if (force[0] == '-' && force[1] == '-') {
            force = force.substr(8);
        }
        // backwards combatabiltiy
	if (force == "srmcp" ) {
            force = "srm";
        }
	if (force == "gridftp" || force == "expftp" || force == "expgridftp") {
            force = "gsiftp";
        }
	if (force == "cp" || force == "dd") {
            force = "file";
        }
	if (force == "root" || force == "xroot" || force == "xrootd") {
            force = "root";
        }
        if (force[force.size()-1] != ':') {
            force = force + ":";
        }
        // only take the force if the src and dst both have it...
        if (srcps.find(force) != std::string::npos && srcps.find(force) != std::string::npos ) {
	    p.proto = force;
	    p.proto2 = "";
	    return;
        }
    }
    std::vector<std::string> slist, dlist;
    // if src or dst has a specified proto, thats the only choice,
    // otherwise we use the list from the location.
    if (p.src.proto != "") { 
        srcps = p.src.proto;
    }
    slist = split(srcps, ' ');
    if (p.dst.proto != "") {
       dstps =  p.dst.proto;
    }
    dlist = split(dstps, ' ');
    ifdh::_debug && cerr << "pick_proto: srcloc: " <<  p.src.location 
                          << " protos: " << srcps << "\n" 
                          << " dstloc: " << p.dst.location 
                          << " protos: " << dstps << "\n" ;
     for( std::vector<std::string>::iterator sp = slist.begin(); sp != slist.end(); sp++) {
         if (*sp == "" || *sp == "\t" || *sp == " ") {
            continue;
         }
         // only take file: if it is actually visible here
         if (*sp == "file:" && 0 != access(parent_dir(p.src.path).c_str(),R_OK)) {
             ifdh::_debug && cerr << "ignoring src file: -- not visible\n";
             continue;
         }
         for( std::vector<std::string>::iterator dp = dlist.begin(); dp != dlist.end(); dp++) {
	     if (*dp == "" || *dp == "\t" || *dp == " ") {
		continue;
	     }
             // only take file: if it is a path actually visible here
             if (*dp == "file:" && 0 != access(parent_dir(p.dst.path).c_str(),R_OK)) {
                 ifdh::_debug && cerr << "ignoring dst file: -- not visible\n";
                 continue;
             }
             if (*sp == *dp || *dp == "file:" ) {
                 p.proto = *sp;
                 p.proto2 = "";
                 ifdh::_debug && cerr << "pick_proto: Picked protocol " << p.proto << "\n";
                 return;
             }
             if (*sp == "file:" ) {
                 p.proto = *dp;
                 p.proto2 = "";
                 ifdh::_debug && cerr << "pick_proto: Picked protocol " << p.proto << "\n";
                 return;
             }
         }
     }
     p.proto = slist[0];
     if (dlist[0] != p.proto) {
         p.proto2 = dlist[0];
         ifdh::_debug && cerr << "Mixed protocol " << p.proto << ", " << p.proto2 <<"\n";
     }
     return;
}

std::vector<CpPair>
handle_args( std::vector<std::string> args, std::vector<std::string>::size_type curarg, bool dest_is_dir,  WimpyConfigParser &_config, size_t &lock_low, size_t &lock_hi, string &force) {

    std::vector<CpPair> res;
    CpPair p;
    IFile l;

    lock_hi = 0;
    lock_low = args.size() + 1;
    size_t lastslot = 0;
    for(curarg = curarg; curarg< args.size(); curarg++) {
        if (args[curarg] == ";") {
           continue;
        } else {
           l = lookup_loc(args[curarg], _config);
        
           if (curarg+1 != args.size() && args[curarg+1] != ";") {
               // its another src
               p.src = l;
               res.push_back(p);
           } else {
               // it is a destination; update all the pending ones
               for( size_t j = lastslot; j< res.size(); j++) {
                   res[j].dst = l;
                   if ( dest_is_dir ) {
                       res[j].dst.path = dest_file(res[j].src.path, res[j].dst.path);
                   }
                   pick_proto(res[j], _config, force);
               }
               lastslot = res.size();
           }
       } 
       if (_config.getint("location " + l.location, "need_cpn_lock") ) {
           if( res.size()-1 < lock_low ) {
               lock_low = res.size()-1;
           }
           if( res.size()-1 > lock_hi ) {
               lock_hi = res.size()-1;
           }
       }
   }
   return res;
}

int 
ifdh::cp( std::vector<std::string> args ) {
    string force = "";
    bool recursive = false;
    bool dest_is_dir = false;
    bool no_zero_length = false;
    bool intermed_file_flag = false;
    bool need_copyback = false;
    bool cleanup_stage = false;
    std::vector<std::string>::size_type curarg = 0;
    std::vector<CpPair> cplist;
    int res = 0, rres = 0;
    long int srcsize = 0, dstsize = 0, xfersize=0;
    struct stat *sbp;
    size_t lock_low, lock_hi;
    cpn_lock cpn;
    struct timeval time_before, time_after;

    for( std::vector<std::string>::size_type i = 0; i < args.size(); i++ ) {
       make_canonical(args[i]);
    }

    gettimeofday(&time_before, 0);
    std::string logmsg("starting ifdh::cp( ");
    logmsg += ifdh_util_ns::join(args, ' ');
    logmsg += ");\n";

    log(logmsg.c_str());
    _debug && std::cerr << logmsg;

    char *stage_via = parse_ifdh_stage_via();
    if (stage_via && !getenv("EXPERIMENT")) {
       _debug && cerr << "ignoring $IFDH_STAGE_VIA: $EXPERIMENT not set\n";
       log("ignoring $IFDH_STAGE_VIA-- $EXPERIMENT not set  ");
       stage_via = 0;
    }
    if (stage_via) {
       std::vector<std::string> svls = ls(parent_dir(stage_via),1,"");
       if (svls.size() == 0) {
           log("ignoring $IFDH_STAGE_VIA-- cannot ls parent directory  ");
           stage_via = 0;
       }
    }

    curarg = parse_opts(args, dest_is_dir, recursive, force, no_zero_length, intermed_file_flag);

    if (stage_via) {
         args = build_stage_list(args, curarg, stage_via);
         cleanup_stage = true;
         need_copyback = true;
    }

    cplist = handle_args(args, curarg, dest_is_dir, _config, lock_low, lock_hi, force);

    curarg = 0;
    for (std::vector<CpPair>::iterator cpp = cplist.begin();  cpp != cplist.end(); cpp++) {
        // try to keep a total copied
        xfersize = -1;
	if (0 != (sbp =  cache_stat(cpp->src.path))) {
		srcsize += sbp->st_size;
                xfersize = sbp->st_size;
        }
	if (0 != (sbp =  cache_stat(cpp->dst.path))) {
		dstsize += sbp->st_size;
                xfersize = sbp->st_size;
        }

        // take  a lock if needed
        if (curarg == lock_low) {
            cpn.lock();
        }

        stringstream cpstartmessage;
        cpstartmessage << "ifdh starting transfer: " << cpp->src.path << ", " << cpp->dst.path << "\n";
        log(cpstartmessage.str());

        // actually do the copy
        res = do_cp(*cpp, _config, intermed_file_flag, recursive, cpn);

        stringstream cpdonemessage;
        cpdonemessage << "ifdh finished transfer: " << cpp->src.path << ", " << cpp->dst.path << " size: " << xfersize << "\n";
        log(cpdonemessage.str());

        // release lock if needed
        if (curarg == lock_hi && lock_hi >= lock_low) {
            cpn.free();
        }

        // update overall success
        if (res != 0 && rres == 0) {
            rres = res;
        }

    }

    gettimeofday(&time_after,0);

    if (rres == 0) {
	long int copysize;
	stringstream logmessage;

	if (srcsize > dstsize) {
	    copysize = srcsize ;
	} else {
	    copysize = dstsize ;
	}
        long int delta_t = time_after.tv_sec - time_before.tv_sec;
	long int delta_ut = time_after.tv_usec - time_before.tv_usec;
	// borrow from seconds if needed
	if (delta_ut < 0) {
	    delta_ut += 1000000;
	    delta_t--;
	}
	double fdelta_t = ((double)delta_ut / 100000.0) + delta_t;
	logmessage << "ifdh cp: transferred: " <<  copysize << " bytes in " <<  fdelta_t << " seconds \n";
	_debug && cerr << logmessage.str();
	log(logmessage.str());
    } else {
        std::cerr << "ifdh cp failed at: " << ctime(&time_after.tv_sec) << endl;
        log("ifdh cp failed.");
    }
    if (cleanup_stage) {
        // try to clean up the stage copy list file
        // we wrote in build_stage_list, which is the
        // src of our last copy...
        (void)unlink(cplist[cplist.size()-1].src.path.c_str());
    }

    if (need_copyback) {
        res =  system("ifdh_copyback.sh");
        if (WIFSIGNALED(res)) throw( std::logic_error("signalled while running copyback"));
    }

    return rres;
}

int
ifdh::mv(vector<string> args) {
    int res;
    vector<string>::iterator p;

    res = cp(args);
    if ( res == 0 ) {
        args.pop_back();
        for (p = args.begin(); p != args.end() ; p++ ) {
            string &s = *p;
            if (s == ";" || *(p+1) == ";" || s[0] == '-') {
                // don't remove destinations, options, or files named ";"
                continue;
            }
            rm(s,"");
        }
    }
    return res;
}

vector<string>
ifdh::ls(string loc, int recursion_depth, string force) {
    vector<string> res;
    std::vector<std::pair<std::string,long> > llout;

    // return just the names from lss's output
    llout = lss(loc, recursion_depth, force );
    for(size_t i = 0; i < llout.size(); i++) { 
       res.push_back(llout[i].first);
    }
    if (res.size() == 0 && llout.capacity() == 1) {
       res.reserve(1);
    }
    return res;
}

void
pick_proto_path(std::string loc, std::string force, std::string &proto, std::string &fullurl, std::string &lookup_proto, WimpyConfigParser &_config) {
    make_canonical(loc);
    
    IFile src = lookup_loc(loc,_config);

    std::string  protos;

    if (force.length() == 0) {
       if (getenv("IFDH_FORCE")) {
          force += "--force=";
          force += getenv("IFDH_FORCE");
       }
    }
    if (force != "") {
        if (force[0] == '-' && force[1] == '-') {
            force = force.substr(8);
        }
        // backwards combatabiltiy
	if (force == "srmcp" ) {
            force = "srm";
        }
	if (force == "gridftp" || force == "expftp" || force == "expgridftp") {
            force = "gsiftp";
        }
	if (force == "cp" || force == "dd") {
            force = "file";
        }
	if (force == "root" || force == "xroot" || force == "xrootd") {
            force = "root";
        }
        if (force[force.size()-1] != ':') {
            force = force + ":";
        }
        proto = force;
    } else if (src.proto != "" ) {
       proto = src.proto;
    } else {
        protos = _config.get("location "+src.location,"protocols");
        ifdh::_debug && std::cerr << "location " <<  src.location << " protocols: " << protos << "\n";
        std::vector<std::string> plist = split(protos, ' ');
        // don't use file: if its not visible..
        if (plist[0] == "file:" && 0 != access(parent_dir(src.path).c_str(),R_OK) && plist.size() > 1) {
            proto = plist[1];
        } else { 
            proto = plist[0]; 
        }
    }
    ifdh::_debug && std::cerr << "picked protocol " << proto << "\n";
     
    size_t pos = proto.find(" ");
    if (pos != std::string::npos) {
         proto = proto.substr(0,pos);
    }

    fullurl = locpath(src, proto, _config);
    lookup_proto = "protocol " + proto;

    if (_config.getint(lookup_proto, "need_proxy") ) {
        get_grid_credentials_if_needed();
    }
}
int
ifdh::ll( std::string loc, int recursion_depth, std::string force) {

    std::string fullurl, proto, lookup_proto;
    pick_proto_path(loc, force, proto, fullurl, lookup_proto, _config);
   
    std::string cmd    = _config.get(lookup_proto, "ll_cmd");
    std::string r, recursive;
    std::stringstream rdbuf;

    // handle default param
    if (recursion_depth == -1) {
         recursion_depth = 1;
    }

    // ---------------------------
    // strings for command replacement
    rdbuf << recursion_depth;

    if (recursion_depth > 1) {
       r = "-r";
       recursive = "--recursive";
    } else {
       r = "";
       recursive = "";
    }
       
    // ... now fill them in...
    
    cmd.replace(cmd.find("%(src)s"), 7, fullurl);
    if ( has(cmd, "%(recursion_depth)s") ) {
        cmd.replace(cmd.find("%(recursion_depth)s"), 19, rdbuf.str());
    }
    if ( has(cmd, "%(r)s") ) {
        cmd.replace(cmd.find("%(r)s"), 5, r);
    }
    if ( has(cmd, "%(recursive)s") ) {
        cmd.replace(cmd.find("%(recursive)s"), 13, recursive);
    }
    // ---------------------------

    _debug && std::cerr << "ifdh ll: running: " << cmd << endl;

    int status = system(cmd.c_str());

    if (WIFSIGNALED(status)) throw( std::logic_error("signalled while doing ll"));
    return WEXITSTATUS(status);
}


int
mypaircmp( std::pair<std::string,long> a, std::pair<std::string,long> b) {
   return a.first < b.first;
}

std::vector<std::pair<std::string,long> > 
ifdh::lss( std::string loc, int recursion_depth, std::string force) {

    std::vector<std::pair<std::string,long> >  res;

    // save original location before messing with it to get full url
    string origloc(loc);

    std::string fullurl, proto, lookup_proto;
    pick_proto_path(loc, force, proto, fullurl, lookup_proto, _config);

    if (-1 == recursion_depth ) {
       recursion_depth = 1;
    }

    std::string lss_cmd     = _config.get(lookup_proto, "lss_cmd");
    std::string lss_re1_str = _config.get(lookup_proto, "lss_re1");
    std::string lss_re2_str = _config.get(lookup_proto, "lss_re2");
    _debug && std::cerr << "re1: " << lss_re1_str << "\n";
    _debug && std::cerr << "re2: " << lss_re2_str << "\n";
    int lss_size_last       = _config.getint(lookup_proto, "lss_size_last");
    int lss_dir_last        = _config.getint(lookup_proto, "lss_dir_last");
    int lss_skip            = _config.getint(lookup_proto, "lss_skip");

    regexp *lss_re[2];
    regexp r1(lss_re1_str);
    regexp r2(lss_re2_str != "" ? lss_re2_str : "xxxxxxx"); // there may not be one...
    lss_re[0] = &r1;
    lss_re[1] = &r2;

    lss_cmd.replace(lss_cmd.find("%(src)s"), 7, fullurl);

    stringstream rdbuf;
    rdbuf << recursion_depth;

    _debug && std::cerr << "running: " << lss_cmd << "\n";

    // no c++ popen, so doing it C-style..
    FILE *pf = popen(lss_cmd.c_str(),"r");
    static char buf[512];
    std::string size, dir, file, dirflag;
    bool firstpass = true;
    std::string dirrepl = origloc + "/";
    // use flags to pick regex submatch numbers for parsing
    int nsize, ndir, nfile, ndirflag;
    // if size is last its dir file size
    if (lss_size_last) {
	nsize = 3;
	ndir  = 1;
	nfile = 2;
    } else {
	nsize = 1;
	ndir  = 2;
	nfile = 3;
    }
    // if dir flag is not last, shove everything else over
    if (lss_dir_last) {
       ndirflag = 4;
    } else {
       ndirflag = 1;
       nsize++;
       ndir++;
       nfile++;
    }

    while (fgets(buf, 512, pf)) {

	_debug && std::cerr << "line: " << buf;
        if (lss_skip) {
           lss_skip--;
	   _debug && std::cerr << "skipping...\n";
           continue;
        }

        for(int  i = 0 ; i < 2 ; i++ ) {
            string sbuf(buf);

            if ('\n' == sbuf[sbuf.size()-1]) {
               sbuf = sbuf.substr(0,sbuf.size()-1);
            }
            if ('\r' == sbuf[sbuf.size()-1]) {
               sbuf = sbuf.substr(0,sbuf.size()-1);
            }
	    regmatch m1((*lss_re[i])(sbuf));
	    if (m1) {
               _debug && std::cerr << "matches:" << m1[1] << "," << m1[2]<< "," <<m1[3] << "," << m1[4] <<";\n";
               size = m1[nsize];
               dir = m1[ndir];
               file = m1[nfile];
               dirflag = m1[ndirflag];

               // we dont include . or ..
               if ("." == file || ".." == file) {
                   continue;
               }

               _debug && std::cerr << "file: " << file << " dir " << dir << " size " << size << "\n";

               if (firstpass) {
                   // if the list doesn't start with the thing we asked for
                   // (i.e it's a directory and shows contents but not dir)
                   // stick the dir on the front.
                   firstpass = false;
                   std::string basename = origloc.substr(origloc.rfind("/",origloc.size()-2)+1);
                   _debug && std::cerr << "basename: |" <<  basename << "| file |" << file << "|\n";
                   if (file != basename) {
                       dirrepl = origloc + "/";
                   } else {
                       dirrepl = origloc.substr(0,origloc.rfind("/", origloc.size()-1)+1);
                       if (dirrepl == "") {
                          dirrepl = dir;
                       }
                   }
		   if ("//" == dirrepl.substr(dirrepl.size()-2) ) {
		      dirrepl = dirrepl.substr(0,dirrepl.size()-1);
                   }
                   if (file != basename && file != "") {
                       res.push_back( pair<string,long>(dirrepl,  0L));
                   }
               }

               if (dir == "") {
                   dir = dirrepl;
               }
               if ((dirflag == "/" || dirflag == "d") && file != "" ) {
                   file = file + "/";
               }
               res.push_back( pair<string,long>(dir + file,  atol(size.c_str())));
               break;
	    } else {
               _debug && std::cerr << "no match.\n";
            }
        }
    }
  
    int status = pclose(pf);

    if (WIFSIGNALED(status)) {
      throw( std::logic_error("signalled while doing lss"));
    } 

    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
  
      _debug && std::cerr << "failed with exit status" << WEXITSTATUS(status) << "\n";
      // res.clear();
      return res;
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0 && res.size() == 0) {
      // empty diretory case, show them the directory
      if (origloc[origloc.size()-1] != '/')
          origloc = origloc + "/";
      res.push_back(pair<string,long>(origloc,  0L));
    }

    std::sort( res.begin(), res.end(), mypaircmp);

    std::vector<std::pair<std::string,long> > subls, recls;

    if (recursion_depth > 1) {

	for(size_t  i = 0 ; i < res.size(); i++ ) {
	    if (i > 0 && res[i].first[res[i].first.size()-1] == '/') {
	       subls = lss(res[i].first, recursion_depth - 1, "");
	       for(size_t  j = 0 ; j < subls.size(); j++ ) {
		  recls.push_back(subls[j]);
	       }
	    } else {
		recls.push_back(res[i]);
	    }
       }

       res = recls;

    }
    return res;
}


// figure out how many directories deep a path is
int count_slashes(std::string loc) {
    size_t start = 0, p;
    int res = 0;

    // if it is a url, start looking further in...
    if (std::string::npos != (p = loc.find("://"))) {
	start = p + 4;
    }
    while (std::string::npos != (p = loc.find("/", start))) {
	res = res + 1;
	start = p + 1;
    }
    return res;
}

int
ifdh::mkdir_p(string loc, string force, int depth) {
   _debug && cerr << "mkdir_p(" << loc << "," << force << "," << "," << depth << ")\n";
   if (depth == -1) {
      // we weren't given a depth, assume first 3 dirs
      // from root must exist -- i.e /nova/app/users or
      // /pnfs/experiment/scratch...
      depth = count_slashes(loc) - 3;
      _debug && cerr << "mkdir_l: depth is " << depth << "\n";
   }
   if (depth == 0) {
      return 0;
   }

   std::vector<std::string> res = ls(loc, 0, force);
   if (res.size() == 0) {
      int m;
      // parent does not exist
      mkdir_p(parent_dir(loc), force, depth - 1 );
      try {
         m = mkdir(loc, force);
      } catch (exception e) {
         m = -1;
      }
      return m;
   } else {
      return 0;
   }
}

int
ifdh::mkdir(string loc, string force) {
    std::string fullurl, proto, lookup_proto;
    pick_proto_path(loc, force, proto, fullurl, lookup_proto, _config);
    int retries;
   
    std::string cmd     = _config.get(lookup_proto, "mkdir_cmd");

    cmd.replace(cmd.find("%(src)s"), 7, fullurl);

    _debug && std::cerr << "running: " << cmd << endl;

    // retry, but only 1 time...
    if (0 != getenv("IFDH_CP_MAXRETRIES")) {
        retries = (atoi(getenv("IFDH_CP_MAXRETRIES")) > 0);
    } else {
        retries = 1;
    }
    cpn_lock locker;
    int status = retry_system(cmd.c_str(), 0, locker, "","","",retries);
    if (WIFSIGNALED(status)) {
      // throw( std::logic_error("signalled while doing mkdir"));
      return -1;
    }
    //  if (WIFEXITED(status) && WEXITSTATUS(status) != 0) throw( std::logic_error("mkdir failed"));
    return WEXITSTATUS(status);
}

int
ifdh::rm(string loc, string force) {
    std::string fullurl, proto, lookup_proto;
    pick_proto_path(loc, force, proto, fullurl, lookup_proto, _config);
   
    std::string cmd     = _config.get(lookup_proto, "rm_cmd");

    cmd.replace(cmd.find("%(src)s"), 7, fullurl);

    _debug && std::cerr << "running: " << cmd << endl;

    int status = system(cmd.c_str());
    if (WIFSIGNALED(status)) throw( std::logic_error("signalled while doing rm"));
    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) throw( std::logic_error("rm failed"));
    return 0;
}

int
ifdh::rmdir(string loc, string force) {
    std::string fullurl, proto, lookup_proto;
    pick_proto_path(loc, force, proto, fullurl, lookup_proto, _config);
   
    std::string cmd     = _config.get(lookup_proto, "rmdir_cmd");

    cmd.replace(cmd.find("%(src)s"), 7, fullurl);

    _debug && std::cerr << "running: " << cmd << endl;

    int status = system(cmd.c_str());
    if (WIFSIGNALED(status)) throw( std::logic_error("signalled while doing rm"));
    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) throw( std::logic_error("rm failed"));
    return 0;
}

std::string
mbits( int n ) {
    std::stringstream res;
    if ( 0 == n )
       res << "NONE";
    if ( n & 04 ) 
       res << 'R';
    if ( n & 02 ) 
       res << 'W';
    if ( n & 01 ) 
       res << 'X';
    return res.str();
}

static string
rwx(string smode) {
   string res("");
 
   int mode = (smode[0] - '0') * 64 + (smode[1] - '0') * 8 + (smode[2] - '0');

   for (int shift=6; shift >= 0 ; shift -= 3) {
      for (int i = 2 ; i >= 0; i--) {
         if ( (mode >> shift) & (1 << i) ) {
            res = res + "xwr"[i];
         } else {
            res = res + '-';
         }
      }
   }
   return res;
}

int
ifdh::chmod(string mode, string loc, string force) {
    std::string fullurl, proto, lookup_proto;
    pick_proto_path(loc, force, proto, fullurl, lookup_proto, _config);
   
    std::string cmd     = _config.get(lookup_proto, "chmod_cmd");


    cmd.replace(cmd.find("%(src)s"), 7, fullurl);
    if ( has(cmd, "%(mode)s") ) {
       cmd.replace(cmd.find("%(mode)s"), 8, mode);
    }
    if ( has(cmd, "%(ubits)s") ) {
       cmd.replace(cmd.find("%(ubits)s"), 9, mode.substr(1,1));
    }
    if ( has(cmd, "%(gbits)s") ) {
       cmd.replace(cmd.find("%(gbits)s"), 9, mode.substr(2,1));
    }
    if ( has(cmd, "%(obits)s") ) {
       cmd.replace(cmd.find("%(obits)s"), 9, mode.substr(3,1));
    }
    if ( has(cmd, "%(rwxmode)s") ) {
       cmd.replace(cmd.find("%(rwxmode)s"), 11, rwx(mode));
    }

    _debug && std::cerr << "running: " << cmd << endl;

    int status = system(cmd.c_str());

    if (WIFSIGNALED(status)) throw( std::logic_error("signalled while doing chmod"));
    // if (WIFEXITED(status) && WEXITSTATUS(status) != 0) throw( std::logic_error("chmod failed"));
    return WEXITSTATUS(status);
}

int
ifdh::pin(string loc, long int secs) {
    std::stringstream secsbuf;
    std::string fullurl, proto, lookup_proto;
    // the only pin interface we have is from srm, so look it up there
    pick_proto_path(loc, "srm:", proto, fullurl, lookup_proto, _config);
   
    std::string cmd     = _config.get(lookup_proto, "pin_cmd");

    cmd.replace(cmd.find("%(src)s"), 7, fullurl);
    secsbuf << secs;
    cmd.replace(cmd.find("%(secs)s"), 8, secsbuf.str());

    _debug && std::cerr << "running: " << cmd << endl;

    int status = system(cmd.c_str());

    if (WIFSIGNALED(status)) throw( std::logic_error("signalled while doing pin"));
    // if (WIFEXITED(status) && WEXITSTATUS(status) != 0) throw( std::logic_error("rmdir failed"));
    return WEXITSTATUS(status);
}
       
int 
ifdh::rename(string loc, string loc2, string force) {
    std::string fullurl, proto, lookup_proto;
    std::string fullurl2, proto2, lookup_proto2;
    pick_proto_path(loc, force, proto, fullurl, lookup_proto, _config);
    pick_proto_path(loc2, force, proto2, fullurl2, lookup_proto2, _config);

    if (proto != proto2) {
        throw( std::logic_error("Cannot rename accros protocols/locations"));
    }
   
    std::string cmd     = _config.get(lookup_proto, "mv_cmd");

    cmd.replace(cmd.find("%(src)s"), 7, fullurl);
    cmd.replace(cmd.find("%(dst)s"), 7, fullurl2);

    _debug && cerr << "running: " << cmd << endl;

    int status = system(cmd.c_str());
    if (WIFSIGNALED(status)) throw( logic_error("signalled while doing rename"));
    //if (WIFEXITED(status) && WEXITSTATUS(status) != 0) throw( logic_error("rename failed"));
    return WEXITSTATUS(status);
}

string
glob_2_re(string s) {
   string res;
   res.push_back('/');
   for(size_t i = 0; i < s.size(); i++ ) {
       if (s[i] == '*') {
          res.push_back('.');
          res.push_back('*');
       } else if (s[i] == '?') {
          res.push_back('.');
       } else if (s[i] == '+' || s[i] == '.' || s[i] == '|') {
          res.push_back('\\');
          res.push_back(s[i]);
       } else {
          res.push_back(s[i]);
       }
   }
   res.push_back('$');
   return res;
}



vector<pair<string,long> > 
ifdh::findMatchingFiles( string path, string glob) {
   vector<pair<string,long> >  res, batch;
   vector<string>  plist, globparts;
   vector<string>  dlist, dlist1;
   string prefix;
   size_t globslice = 0;
   string sep;

   prefix = "";
   dlist1 = split(path,':',false);


   // splitting on colons breaks urls, so put them back
   for (size_t i = 0; i < dlist1.size(); ++i) {
       if (dlist1[i] == "srm" || dlist1[i] == "gsiftp" || dlist1[i] == "http"|| dlist1[i] == "s3" || dlist1[i] == "i") {
            prefix = dlist1[i] + ':';
       } else {
            dlist.push_back(prefix + dlist1[i]);
            prefix = "";
       }
   }

   //
   // to prevent excessive recursive directory searching,
   // move constant leading directories from glob onto path components.
   //
   globparts = split(glob,'/');
   for (size_t i = 0; i < globparts.size(); ++i) {
       if ( globparts[i].find('?') != string::npos || globparts[i].find('*') != string::npos ) {
           globslice = i;
       } else {
           for (size_t j = 0; j < dlist1.size(); ++j) {
               dlist[j] = dlist[j] + '/' + globparts[i];
           }
       }
   }
   glob = "";
   sep = "";
   for (size_t i = globslice; i < globparts.size(); ++i) {
       glob += sep;
       glob += globparts[i];
       sep = "/";
   }

   glob = glob_2_re(glob);

   for (size_t i = 0; i < dlist.size(); ++i) {
        if (_debug) cerr << "checking dir: " << dlist[i] << endl;
        try {
            batch = lss(dlist[i],10,"");
        } catch ( exception &e ) {
            continue;
        }
        for(size_t j = 0; j < batch.size(); j++ ) {
            if (_debug) cerr << "checking file: " << batch[j].first << endl;
            regexp globre(glob);
            regmatch m(globre(batch[j].first));
            if (m) {
                res.push_back(batch[j]);
            }
        }
   }
   return res;
}

vector<pair<string,long> > 
ifdh::fetchSharedFiles( vector<pair<string,long> > list, string schema ) {
   vector<pair<string,long> >  res;
   string f;
   string rdpath("root://fndca1.fnal.gov:1094/pnfs/fnal.gov");
   if (getenv("IFDH_STASH_CACHE")) {
       rdpath = getenv("IFDH_STASH_CACHE");
       if (rdpath.find("root:") == 0) {
            schema = "xrootd";
       }
   }

   if (_debug) cerr << "fetchShared -- schema is " << schema << endl;

   for(size_t i = 0; i < list.size(); i++ ) {
       if (_debug) cerr << "fetchShared item " << i << " " << list[i].first << endl;
       // for now, if we found it via a cvmfs path, return it, else
       // run it through ifdh::fetchInput to get a local path
       if (list[i].first.find("/cvmfs/") == 0 ) {
           f = list[i].first;
       } else {
           if (schema == "xrootd" && list[i].first.find("/pnfs") == 0) {
               f = fetchInput(rdpath + list[i].first);
           } else {
               f = fetchInput( list[i].first );
           }
       }
       res.push_back( pair<string,long>(f, list[i].second));
   }
   return res;
}

}
