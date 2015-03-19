
#include "ifdh.h"
#include "utils.h"
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

#include <stdlib.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/vfs.h> 
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <wait.h>
#include <linux/nfs_fs.h>
#include <ifaddrs.h>
#include <../util/regwrap.h>

using namespace std;

namespace ifdh_ns {

std::string bestman_srm_uri = "srm://fg-bestman1.fnal.gov:10443/srm/v2/server?SFN=";
std::string bestman_ftp_uri = "gsiftp://fg-bestman1.fnal.gov:2811";
std::string pnfs_srm_uri = "srm://fndca1.fnal.gov:8443/srm/managerv2?SFN=/pnfs/fnal.gov/usr/";
#ifndef NOSYMLINKS
std::string pnfs_gsiftp_uri = "gsiftp://fndca1.fnal.gov/pnfs/fnal.gov/usr/";
#else
std::string pnfs_gsiftp_uri = "gsiftp://fndca1.fnal.gov/";
#endif
std::string pnfs_cdf_srm_uri = "srm://cdfdca1.fnal.gov:8443/srm/managerv2?SFN=/pnfs/fnal.gov/usr/";
std::string pnfs_cdf_gsiftp_uri = "gsiftp://cdfdca1.fnal.gov/";
std::string pnfs_d0_srm_uri = "srm://d0dca1.fnal.gov:8443/srm/managerv2?SFN=/pnfs/fnal.gov/usr/";
std::string pnfs_d0_gsiftp_uri = "gsiftp://d0dca1.fnal.gov/";


bool has(std::string s1, std::string s2) {
   return s1.find(s2) != std::string::npos;
}

//
// string constant for number of streams for gridftp, srmcp
// 


int
local_access(const char *path, int mode) {
    static struct statfs buf;
    int res;

    
    ifdh::_debug && std::cerr << "local_access(" << path << " , " << mode ;
    res = statfs(path, &buf);
    if (0 != res ) {
       return res;
    }
    if (buf.f_type == NFS_SUPER_MAGIC) {
       ifdh::_debug && std::cerr << ") -- not local \n";
       return -1;
    } else {
       res = access(path, mode);
       ifdh::_debug && std::cerr << ") -- local returning " <<  res << endl;
       return res;
    }
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
is_directory(std::string dirname) {
    struct stat *sbp = cache_stat(dirname);
    if (!sbp) {
        return false;
    }
    ifdh::_debug && std::cerr << "is_directory(" << dirname << ") -> " <<  S_ISDIR(sbp->st_mode) << endl;
    return S_ISDIR(sbp->st_mode) != 0;
}

bool
have_stage_subdirs(std::string uri) {
   std::stringstream cmd;
   static char buf[512];
   int count = 0;
   FILE *pf;

   ifdh::_debug && cerr << "checking with srmls\n";
   cmd << "srmls -2  " << uri;
   pf = popen(cmd.str().c_str(),"r");
   while (fgets(buf, 512, pf)) {
       if(0 != strstr(buf,"ifdh_stage/queue/")) count++;
       if(0 != strstr(buf,"ifdh_stage/lock/")) count++;
       if(0 != strstr(buf,"ifdh_stage/data/")) count++;
   }
   pclose(pf);
   return count == 3;
}

bool 
is_bestman_server(std::string uri) {
   std::stringstream cmd;
   static char buf[512];
   FILE *pf;

   // if it starts with a slash, we're rewriting it to 
   // the fermi bestman server...
   if (uri[0] == '/') 
        return true;

   ifdh::_debug && cerr << "checking with srmping\n";
   cmd << "srmping -2 " << uri;
   pf = popen(cmd.str().c_str(),"r");
   bool found = false;
   while (fgets(buf, 512, pf)) {
       ifdh::_debug && cerr << "srmping says: " << buf << endl;
       if (0 == strncmp("backend_type:BeStMan", buf, 20)) {
           found = true; 
           break;
       }
   }
   pclose(pf);
   return found;
}

bool 
ping_se(std::string uri) {
   std::stringstream cmd;
   int res = 0;

   if (has(uri,"srm:")) {
   ifdh::_debug && cerr << "checking " << uri << " with srmping\n";
   cmd << "srmping -2 -retry_num=2 " << uri;
   res = system(cmd.str().c_str());
   if (WIFSIGNALED(res)) {
       throw( std::logic_error("signalled while doing srmping"));
   }
   }
   return res == 0;
}

#include <dirent.h>
int
is_empty(std::string dirname) {
    DIR *pd = opendir(dirname.c_str());
    struct dirent *pde;
    int count = 0;

    while( 0 != (pde = readdir(pd)) ) {
        count++;
        if (count > 2) {
            break;
        }
    }
    closedir(pd);
    ifdh::_debug && cerr << "is_empty " << dirname << "is " << (count > 2) << endl;
    return count == 2;
}

extern std::string datadir();

// file io

class cpn_lock {

private:

    int _heartbeat_pid;

public:

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
        if (!((WIFSIGNALED(res) && 9 == WTERMSIG(res)) || (WIFEXITED(res) &&WEXITSTATUS(res)==0))) {
            stringstream basemessage;
            basemessage <<"lock touch process exited code " << res << " signalled: " << WIFSIGNALED(res) << " signal: " << WTERMSIG(res);
            throw( std::logic_error(basemessage.str()));
        }
        if (WIFSIGNALED(res2)) {
            throw( std::logic_error("signalled while doing srmping"));
        }
    }

    cpn_lock() : _heartbeat_pid(-1) { ; }
 
    ~cpn_lock()  {

       if (_heartbeat_pid != -1) {
           free();
       }
    }
};

std::vector<std::string> expandfile( std::string fname, std::vector<std::string> oldargs, unsigned int oldcount) {
  std::vector<std::string> res;
  std::string line;
  size_t pos, pos2;

  bool first = true;

  fstream listf(fname.c_str(), fstream::in);
  getline(listf, line);
  while( !listf.eof() && !listf.fail()) {
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
      }
      getline(listf, line);
  }
  if ( !listf.eof() ) {
      std::string basemessage("error reading list of files file: ");
      throw( std::logic_error(basemessage += fname));
  }
  while (oldcount < oldargs.size()) {
     res.push_back(oldargs[oldcount++]);
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

void
test_dest_file() {
    std::string src("/tmp/f1");
    std::string ddir("/tmp/d2");
    std::cout << "src: " << src << " ddir: " << ddir << " yeilds: " << dest_file(src,ddir) << endl;
}


std::vector<std::string> slice_directories(std::vector<std::string> args, int curarg) {
    std::vector<std::string> res;
    std::vector<std::vector<std::string>::size_type> dest_slots;
    
    //
    // find destination directory slots
    //    
    for( std::vector<std::string>::size_type i = curarg; i < args.size(); i++ ) {
        if ( i == args.size() - 1 || args[i+1] == ";" ) {
             dest_slots.push_back(i);
        }
    }

    ifdh::_debug && std::cerr << "slice_diretctories: building results:";
    //
    // now use destination directory slots to make
    // pairwise copies
    //
    int cur_cp = 0;  // start with the zeroth copy
    for( std::vector<std::string>::size_type i = curarg; i < args.size(); i++ ) {
          if (args[i] == ";") {
              cur_cp++;  // if we see a ";" we move on to next copy 
              continue;
          }
          if( i != dest_slots[cur_cp] ) {  // don't do dest dest ";" 
              res.push_back(args[i]);   
              ifdh::_debug && std::cerr << res.back() << " "; 
              res.push_back(dest_file(args[i], args[dest_slots[cur_cp]]));   
              ifdh::_debug && std::cerr << res.back() << " ";
              if (i != args.size() - 2) {
                  res.push_back(";");
                  ifdh::_debug && std::cerr << res.back() << " ";
              }
          }
     }
     ifdh::_debug && std::cerr << endl;

     return res;
}

//
// globus_url_copy will not do recursive directory copies
// without a trailing slash on the url... sigh
//
std::string fix_recursive_arg(std::string arg, bool recursive) {
   if (recursive && arg[arg.length()-1] != '/') {
       arg = arg + "/";
   }
   return arg;
}

std::vector<std::string> 
ifdh::build_stage_list(std::vector<std::string> args, int curarg, char *stage_via) {
   std::vector<std::string>  res;
   std::string stagefile("stage_");
   std::string ustring;
   std::string stage_location;

   // unique string for this stage out
   ustring = unique_string();

   // if we are told how to stage, use it, fall back to OSG_SITE_WRITE,
   //  or worst case, pnfs scratch
   std::string base_uri(stage_via? stage_via : pnfs_srm_uri + getexperiment() + "/scratch/ifdh_stage/");
   if (base_uri[0] == '$') {
       base_uri = base_uri.substr(1);
       base_uri = getenv(base_uri.c_str());
   }
   base_uri = (base_uri + "/" + getexperiment());
   stagefile += ustring;

   // double slashes past the first srm:// break srmls
   // so clean any other double slashe out.
   size_t dspos = base_uri.rfind("//");
   while (dspos != string::npos && dspos > 7) {
      base_uri = base_uri.substr(0,dspos) + base_uri.substr(dspos+1);
      dspos = base_uri.rfind("//");
   }

   // make sure directory hierarchy is there..
   if (!have_stage_subdirs(base_uri + "/ifdh_stage")) {
       this->mkdir( base_uri + "/ifdh_stage", "");
       this->mkdir( base_uri + "/ifdh_stage/queue" , "");
       this->mkdir( base_uri + "/ifdh_stage/lock", "");
       this->mkdir( base_uri + "/ifdh_stage/data", "");
   }

   this->mkdir( base_uri + "/ifdh_stage/data/" + ustring, "");

   // open our stageout queue file/copy back instructions
   fstream stageout(stagefile.c_str(), fstream::out);

   bool staging_out = false;

   for( std::vector<std::string>::size_type i = curarg; i < args.size(); i+=3 ) {

     if (0 == local_access(args[i].c_str(), R_OK) && 0 != local_access(args[i+1].c_str(), R_OK)) {
       staging_out = true;
       // we're going to keep this in our stage queue area
       stage_location = base_uri + "/ifdh_stage/data/" + ustring + "/" +  basename(args[i].c_str());

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
      std::string fullstage(get_current_dir_name());
      fullstage += "/" +  stagefile;
      res.push_back( fullstage );
      res.push_back( base_uri + "/ifdh_stage/queue/" + stagefile );
   } else {
      res.pop_back();
   }

   return res;
}

const char *srm_copy_command = "lcg-cp  --sendreceive-timeout 4000 -b -D srmv2 ";

bool 
check_grid_credentials() {
    static char buf[512];
    FILE *pf = popen("voms-proxy-info -all 2>/dev/null", "r");
    bool found = false;
    std::string experiment(getexperiment());
    std::string path;
    
    ifdh::_debug && std::cerr << "check_grid_credentials:\n";


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
        cmd += "&& voms-proxy-init -rfc -noregen -debug -voms ";
	if (experiment != "lbne" && experiment != "dzero" && experiment != "cdf" && experiment != "lsst" && experiment != "fermilab") {
	   cmd += "fermilab:/fermilab/" + experiment + "/Role=Analysis";
	} else if (experiment == "dzero" ) {
           // dzero has an extra "users" component
	   cmd +=  experiment + ":/" + experiment  + "/users/Role=Analysis";
	} else {
	   cmd += experiment + ":/" + experiment + "/Role=Analysis";
	}
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
            std::cerr << "Unable to get proxy cert.. later things may fail\n";
        }
    }
}
 

int 
host_matches(std::string hostglob) {
   // match trivial globs for now: *.foo.bar
   char namebuf[512];
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

bool
use_passive() {
   return true;
   
   // I was thinking of something like: 
   if (host_matches("*.fnal.gov")) {
       return false;
   } else {
       return true;
   }
   // but even that is wrong sometimes; we should just
   // do passive mode because it works everywhere, and let
   // people use IFDH_GRIDFTP_EXTRA to override it if needed.
}

string
map_pnfs(string loc, int srmflag = 0) {

      bool cdfflag = false;
      bool d0flag = false;
      std::string srmuri, gsiftpuri;

      if (0L == loc.find("/pnfs/fnal.gov/usr/")) {
	  loc = loc.substr(19);
      } else if (0L == loc.find("/pnfs/usr/")) {
	  loc = loc.substr(10);
      } else if (0L == loc.find("/pnfs/fnal.gov/")) {
	  loc = loc.substr(15);
      } else { // must have /pnfs/... to get here at all
	  loc = loc.substr(6);
      }

      if (0L == loc.find("cdfen/")) 
	    cdfflag = true;

      if (0L == loc.find("d0en/")) 
	    d0flag = true;
      
      if (cdfflag) {
          srmuri = pnfs_cdf_srm_uri;
          gsiftpuri = pnfs_cdf_gsiftp_uri;
      } else if ( d0flag ) {
          srmuri = pnfs_d0_srm_uri;
          gsiftpuri = pnfs_d0_gsiftp_uri;
      } else {
          srmuri = pnfs_srm_uri;
          gsiftpuri = pnfs_gsiftp_uri;
      }

      if (srmflag) {
	 loc = srmuri + loc;
      } else {
#ifdef NOSYMLINKS
	  loc = loc.substr(loc.find("/",1)+1);
#endif
	  loc = gsiftpuri + loc;
      } 

      ifdh::_debug && std::cerr << "ending up with pnfs uri of " <<  loc << endl;
      return loc;
}

bool
is_dzero_node_path( std::string path ) {
 // it could be a clued0 node, or it could be a d0srv node...
 // and the d0srv is either at the front, or has user@ on the
 // front of it...
 return path.find("D0:") == 0;
}

int 
ifdh::cp( std::vector<std::string> args ) {

    std::string gftpHost("if-gridftp-");
    std::vector<std::string>::size_type curarg = 0;
    string force = " ";
    cpn_lock cpn;
    int res;
    int rres = 0;
    bool recursive = false;
    bool dest_is_dir = false;
    bool cleanup_stage = false;
    bool no_zero_length = false;
    struct timeval time_before, time_after;


    if (_debug) {
         std::cerr << "entering ifdh::cp( ";
         for( std::vector<std::string>::size_type i = 0; i < args.size(); i++ ) {
             std::cerr << args[i] << " ";
         }
    }

    if (args.size() == 0)
       return 0;

    if (getenv("IFDH_FORCE")) {
        force = getenv("IFDH_FORCE");
    }
    if (getenv("IFDH_NO_ZERO_LENGTH")) {
        no_zero_length = true;
    }

    //
    // parse arguments
    //
    while (curarg < args.size() && args[curarg][0] == '-') {

        //
        // first any --long args
        //
        if (args[curarg].substr(0,8) == "--force=") {
           force = args[curarg].substr(8,1);
           _debug && cerr << "force option is " << args[curarg] << " char is " << force << endl;
           curarg++;
           continue;
        }
        if (args[curarg] == "--no_zero_length") {
           no_zero_length = true;
           curarg++;
           continue;
        }

        //
        // now any longer args
        //
	if (args[curarg].find('D') != std::string::npos) {
           dest_is_dir = true;
        }

	if (args[curarg].find('r')  != std::string::npos) {
           recursive = true;
        }
 
        // 
        // handle -f last, 'cause it rewrites arg list
        //
	if (args[curarg].find('f') != std::string::npos) {
	   args = expandfile(args[curarg + 1], args, curarg+2);
	   curarg = 0;
           // cout << "after -f args is: ";
           // for (unsigned int k = 0; k< args.size(); k++) {
           //     cout << '"' << args[k] << '"' << ' ' ;
           // }
	   // cout << '\n';
           continue;
	}

        curarg++;
    }
   

    // convert relative paths to absolute
    
    string cwd(get_current_dir_name());

    if (cwd[0] != '/') {
        throw( std::logic_error("unable to determine current working directory"));
    }

    for( std::vector<std::string>::size_type i = curarg; i < args.size(); i++ ) {

       if (args[i][0] != ';' && args[i][0] != '/' && args[i].find("srm:") != 0 && args[i].find("gsiftp:") != 0 && !is_dzero_node_path(args[i])) {
           _debug && std::cerr << "adding cwd to " << args[i] << endl;
	   args[i] = cwd + "/" + args[i];
       }
       //
       // for now, handle pnfs paths via gridftp, unless srm is specified
       // 
       if (0L == args[i].find("/pnfs/")) {
            args[i] = map_pnfs(args[i],force[0]=='s');
       }
    }

    
    // now decide whether to get a cpn lock...
    bool need_cpn_lock = false;
    int need_lock_low = args.size() + 1;
    int need_lock_high = -1;
    for( std::vector<std::string>::size_type i = curarg; i < args.size(); i++ ) {
        if (args[i] == ";") {
           continue;
        }

        if (args[i].find("/grid") == 0) {
           _debug && cerr << "need lock: " << args[i] << " /grid test\n";
 	   need_cpn_lock = true;
	   if ((int)i < need_lock_low) 
	       need_lock_low = i;
	   if ((int)i > need_lock_high)
	       need_lock_high = i;
        }

        if (args[i][0] == '/' && 0 == access(parent_dir(args[i]).c_str(),R_OK) && 0 != local_access(parent_dir(args[i]).c_str(),R_OK) && 0L != args[i].find("/pnfs") && 0l != args[i].find("/scratch") ) {
           _debug && cerr << "need lock: " << args[i] << " NFS test\n";
           // we can see it but it's not local and not /pnfs
           //   and it isn't a nfs mounted /scratch area
           need_cpn_lock = true;
	   if ((int)i < need_lock_low) 
	       need_lock_low = i;
	   if ((int)i > need_lock_high)
	       need_lock_high = i;
        }

        if (args[i].find("gsiftp://if-gridftp") == 0 ) {
           _debug && cerr << "need lock: " << args[i] << " if-gridftp test\n";
 	   need_cpn_lock = true;
	   if ((int)i < need_lock_low) 
	       need_lock_low = i;
	   if ((int)i > need_lock_high)
	       need_lock_high = i;
        }

        if (args[i].find(bestman_srm_uri) == 0) {
           _debug && cerr << "need lock: " << args[i] << " bestman_srm test\n";
 	   need_cpn_lock = true;
	   if ((int)i < need_lock_low) 
	       need_lock_low = i;
	   if ((int)i > need_lock_high)
	       need_lock_high = i;
        }

        if (args[i].find(bestman_ftp_uri) == 0) {
           _debug && cerr << "need lock: " << args[i] << " bestman_ftp test\n";
 	   need_cpn_lock = true;
	   if ((int)i < need_lock_low) 
	       need_lock_low = i;
	   if ((int)i > need_lock_high)
	       need_lock_high = i;
        }
    }


    // now decide local/remote
    // if anything is not local, use remote
    bool clued0_hack = false;
    bool use_srm = false;
    bool use_exp_gridftp = false;
    bool use_bst_gridftp = false;
    bool use_cpn = true;
    bool use_dd = false;
    bool use_any_gridftp = false;
    bool use_irods = false;
    bool use_s3 = false;

    char *stage_via = parse_ifdh_stage_via();

    if (stage_via && !ping_se(stage_via)) {
       _debug && cerr << "ignoring $IFDH_STAGE_VIA due to ping failure \n";
       this->log("ignoring $IFDH_STAGE_VIA due to ping failure");
       this->log(stage_via);
       stage_via = 0;
    }
    if (stage_via && !getenv("EXPERIMENT")) {
       _debug && cerr << "ignoring $IFDH_STAGE_VIA: $EXPERIMENT not set\n";
       this->log("ignoring $IFDH_STAGE_VIA-- $EXPERIMENT not set  ");
       stage_via = 0;
    }

    if (force[0] == ' ') { // not forcing anything, so look

	for( std::vector<std::string>::size_type i = curarg; i < args.size(); i++ ) {

            if( args[i] == ";" ) {
                continue;
            }

            if( is_dzero_node_path(args[i]) )  { 
               // if we're given a foo-clued0:/path we're going
               // to use rsync to copy it later, and we're going
               // to use a per-host cpn lock group...
               _debug && std::cerr << "In clued0 hack case..\n";
               std::string cpngroup("dzero/");
               clued0_hack = true;
               use_cpn = false; 
               use_srm = false;
 	       need_cpn_lock = true;
               if ((int)i < need_lock_low) 
                   need_lock_low = i;
               if ((int)i > need_lock_high)
                   need_lock_high = i;
               cpngroup += args[i].substr(3,args[i].find(':',3)-3);
               setenv("CPN_LOCK_GROUP", cpngroup.c_str(),0);
               break;
            }

            if( args[i].find("s3:") == 0)  { 
               use_cpn = false; 
               use_srm = false;
               use_s3 = true; 
               break; 
            }
            if( args[i].find("i:") == 0)  { 
               use_cpn = false; 
               use_srm = false;
               use_irods = true; 
               break; 
            }
            if( args[i].find("srm:") == 0)  { 
               use_cpn = false; 
               use_srm = true; 
               break; 
            }

            if( args[i].find("gsiftp:") == 0) {
                use_cpn = false; 
                use_srm = false;
		if ( i == args.size() - 1 || args[i+1] == ";" ) {
                    _debug && cerr << "deciding to use bestman gridftp due to " << args[i] << " \n";
                    // our destination is a specified gridftp server
                    // so use bestman for (input) rewrites
                    use_bst_gridftp = true; 
                } else {
                    // our source is a specified gridftp server
                    // so use per-experiment gridftp for (output) rewrites
                    _debug && cerr << "deciding to use exp gridftp due to " << args[i] << " \n";
                    use_exp_gridftp = true; 
                }
                break; 
            }

	    if( 0 != access(args[i].c_str(),R_OK) ) {

                string altmount  = "/.ifdh_only" + args[i];

                if ( 0 == access(altmount.c_str(),R_OK) ) {
                     args[i] = altmount;
                     need_cpn_lock = true;
		     if ((int)i < need_lock_low) 
			   need_lock_low = i;
		     if ((int)i > need_lock_high)
			   need_lock_high = i;
                     continue;
                }
	       
		if ( i == args.size() - 1 || args[i+1] == ";" ) {


		   if (0 != access(parent_dir(args[i]).c_str(),R_OK)) {
		       // if last one (destination)  and parent isn't 
		       // local either default to per-experiment gridftp 
		       // to get desired ownership. 
		       use_cpn = 0;
                           
                       if (stage_via && has(stage_via,"srm:")) {
                           use_srm = 1;
                           _debug && cerr << "deciding to use srm due to $IFDH_STAGE_VIA and: " << args[i] << endl;
                       } else {
		           use_exp_gridftp = 1;
                           _debug && cerr << "deciding to use exp gridftp due to: " << args[i] << endl;
                       }
		   }  
		} else {
		   // for non-local sources, default to srm, for throttling (?)
		   use_cpn = 0;
		   use_srm = 1;
	           _debug && cerr << "deciding to use bestman to: " << args[i] << endl;
		}
                // don't break out here, go back around because 
                // we might get a more specific behavior override 
                // than this..
	     }
	 }
     } else if (force[0] == 's' && force[1] == '3' ) {
         use_cpn = false;
         use_s3 = true;
     } else if (force[0] == 'i') {
         use_cpn = false;
         use_irods = true;
     } else if (force[0] == 'd') {
         use_cpn = false;
         use_dd = true;
     } else if (force[0] == 's') {
         use_cpn = false;
         use_srm = true;
     } else if (force[0] == 'g') {
         use_cpn = false;
         use_bst_gridftp = true;
     } else if (force[0] == 'e') {
         use_cpn = false;
         use_exp_gridftp = true;
     } else if (force[0] == 'c') { 
          use_cpn = true;
          use_srm = false;
          use_exp_gridftp = false;
          use_bst_gridftp = false;
          ;   // forcing CPN, already set
     } else {
          std::string basemessage("invalid --force= option: ");
          throw( std::logic_error(basemessage += force));
     }

     use_any_gridftp = use_any_gridftp || use_exp_gridftp || use_bst_gridftp;

     if (recursive && (use_srm || use_dd )) { 
        throw( std::logic_error("invalid use of -r with --force=srm or --force=dd"));
     }


     //
     // default to dd for non-recursive copies.
     // 
     if (use_cpn && !recursive) {
         use_cpn = 0;
         use_dd = 1;
     }

     if (use_exp_gridftp) {
        gftpHost.append(getexperiment());
        gftpHost.append(".fnal.gov");
     }

     //
     // srmcp and dd only do specific srcfile,destfile copies
     //
     if (dest_is_dir && (use_srm || use_dd || use_any_gridftp)) {
         args = slice_directories(args, curarg);
         dest_is_dir = false;
         curarg = 0;
     }

     if ( stage_via ) {
         args = build_stage_list(args, curarg, stage_via);
	 // we now have a stage back file to clean up later...
         cleanup_stage = true;
         curarg = 0;
         // workaround srmls heap limit hang problems
         setenv("SRM_JAVA_OPTIONS", "-Xmx1024m" ,0);
     }

     int error_expected;
     int keep_going = 1;
 
     // get the proxy before we get the lock, so the
     // lock file will have the username info right.
     
     if (use_any_gridftp || use_srm || use_irods ) {
	get_grid_credentials_if_needed();
     }


     // this looks redudnant, but the proxy could have 
     // expired while we were waiting for a lock...
     //
     if (use_any_gridftp || use_srm || use_irods ) {
	get_grid_credentials_if_needed();
     }

     gettimeofday(&time_before, 0);

     bool need_copyback = false;

     long int srcsize = 0, dstsize = 0;
     struct stat *sbp;

     while( keep_going ) {
         stringstream cmd;

         cmd << (use_dd ? "dd bs=512k " : use_cpn ? "cp "  : use_srm ? srm_copy_command  : use_any_gridftp ? "globus-url-copy -gridftp2 -nodcau -restart -stall-timeout 14400 " :  use_irods ? "icp " :  clued0_hack ? "scp " : "false" );

         if (use_any_gridftp) {
            if ( use_passive()) {
                cmd << " -dp ";
            } else {
                cmd << " -p 4 ";
            }
         }

         if (use_dd && getenv("IFDH_DD_EXTRA")) {
            cmd << getenv("IFDH_DD_EXTRA") << " ";
         }
         if (use_cpn && getenv("IFDH_CP_EXTRA")) {
            cmd << getenv("IFDH_CP_EXTRA") << " ";
         }
         if (use_srm && getenv("IFDH_SRM_EXTRA")) {
            cmd << getenv("IFDH_SRM_EXTRA") << " ";
         }
         if (use_irods && getenv("IFDH_IRODS_EXTRA")) {
            cmd << getenv("IFDH_IRODS_EXTRA") << " ";
         }
         if (use_s3 && getenv("IFDH_S3_EXTRA")) {
            cmd << getenv("IFDH_IRODS_EXTRA") << " ";
         }
         if (use_any_gridftp && getenv("IFDH_GRIDFTP_EXTRA")) {
            cmd << getenv("IFDH_GRIDFTP_EXTRA") << " ";
         }

         if (recursive) {
            cmd << "-r ";
         }

         if ((recursive || dest_is_dir) && use_any_gridftp) {
             cmd << "-cd ";
         }

         error_expected  = 0;

         // check if source is local and zero length, and skip if 
         // we were told to not copy empty files.
	 if (no_zero_length && 0 == local_access(args[curarg].c_str(), R_OK) ) {
	     sbp = cache_stat(args[curarg]);
	     if (sbp && sbp->st_size == 0) {
	 	std::cerr << "Not copying " << args[curarg] << " as it is zero length.\n";
	 	continue;
	     }
	 }

         bool did_one_endpoint = false;

         while (curarg < args.size() && args[curarg] != ";" ) {

            if ((int)curarg == need_lock_low) {
                cpn.lock();
            }

            args[curarg] = fix_recursive_arg(args[curarg],recursive);

            if (is_directory(args[curarg])) {

		if ( use_bst_gridftp || use_exp_gridftp ) {
		    if ( is_empty(args[curarg])) {
			ifdh::_debug && std::cerr << "expecting error due to empty dir " << args[curarg] << endl;
				error_expected = 1;
		    }
		    if ( args[curarg][args[curarg].length()-1] != '/' ) {
			args[curarg] += "/";
		    }
		}

                // emulate globus-url-copy directory behavior of doing 1-level
                // deep copy for cpn by doing dir/*
                if (use_cpn && !recursive && curarg != args.size() - 1 && args[curarg+1] != ";" && !is_empty(args[curarg]) ) {
                    args[curarg] += "/*";   
                }

            }

	    if (use_any_gridftp && dest_is_dir && (curarg == args.size() - 1 || args[curarg+1] == ";" ) &&  args[curarg][args[curarg].length()-1] != '/' ) {
		 
		    args[curarg] += "/";
	    }

            if (use_cpn ) { 
                // no need to munge arguments, take them as is.
                cmd << args[curarg] << " ";
            } else if (clued0_hack) { 
                // stick in $GRID_USER@host:path if no user provided
                // and we have $GRID_USER...

                if ( is_dzero_node_path(args[curarg]) && args[curarg].find('@') == std::string::npos && getenv("GRID_USER")) {
                    cmd <<  getenv("GRID_USER") << "@" << args[curarg].substr(3) << " ";
                } else if (is_dzero_node_path(args[curarg]) ) {
                   cmd << args[curarg].substr(3) << " ";
                } else {
                   cmd << args[curarg] << " ";
                }
            } else if (use_dd) {
                if (curarg == args.size() - 1 || args[curarg+1] == ";" ){
                   cmd << "of=" << args[curarg] << " ";
                } else {
                   cmd << "if=" << args[curarg] << " ";
                }
            } else if (0 == local_access(args[curarg].c_str(), R_OK)) {
                cmd << "file:///" << args[curarg] << " ";
            } else if (( curarg == args.size() - 1 || args[curarg+1] == ";" ) && (0 == local_access(parent_dir(args[curarg]).c_str(), R_OK))) {
                cmd << "file:///" << args[curarg] << " ";
            } else if (use_srm) {
                if (stage_via) {
                    need_copyback = true;
                }

                if( curarg + 1 < args.size() && args[curarg+1].find("srm:") == 0) {
                    did_one_endpoint = 1;
                }
                if( args[curarg].find("srm:") == 0) {
		    cmd << args[curarg] << " ";
                    did_one_endpoint = 1;
                } else if ( did_one_endpoint && 0 == access(parent_dir(args[curarg]).c_str(), R_OK)) {
                    // if one endpoint is visible locally and the other is srm: use it locally
                    cmd << "file:///" << args[curarg] << " ";
                } else {
		    cmd << bestman_srm_uri << args[curarg] << " ";
                    did_one_endpoint = 1;
                }


            } else if (use_exp_gridftp) {
                if( curarg + 1 < args.size() && args[curarg+1].find("gsiftp:") == 0) {
                    did_one_endpoint = 1;
                }
                if( args[curarg].find("gsiftp:") == 0) {
                    cmd << args[curarg] << " ";
                    did_one_endpoint = 1;
                } else if ( did_one_endpoint && 0 == access(parent_dir(args[curarg]).c_str(), R_OK)) {
                    cmd << "file:///" << args[curarg] << " ";
                } else {
                    cmd << "gsiftp://" << gftpHost << args[curarg] << " ";
                    did_one_endpoint = 1;
                }
            } else if (use_bst_gridftp) {
                if( curarg + 1 < args.size() && args[curarg+1].find("gsiftp:") == 0) {
                    did_one_endpoint = 1;
                }
                if( args[curarg].find("gsiftp:") == 0) {
                    cmd << args[curarg] << " ";
                    did_one_endpoint = 1;
                } else if ( did_one_endpoint && 0 == access(parent_dir(args[curarg]).c_str(), R_OK)) {
                    cmd << "file:///" << args[curarg] << " ";
                } else {
                    cmd << bestman_ftp_uri << args[curarg] << " ";
                    did_one_endpoint = 1;
                }
            } else if ( use_irods || use_s3 ) {
	        cmd << args[curarg] << " ";
            }
            curarg++;
        }
	if (need_cpn_lock && (int)curarg > need_lock_high) {
            need_cpn_lock = false;
            cpn.free();
        }

        _debug && std::cerr << "running: " << cmd.str() << endl;

        res = system(cmd.str().c_str());
        if (WIFEXITED(res)) {
            res = WEXITSTATUS(res);
        } else {
            std::cerr << "program: " << cmd.str() << " died from signal " << WTERMSIG(res) << "-- exiting.\n";
            exit(-1);
        }
       
        if ( res != 0 && error_expected ) {
            _debug && std::cerr << "expected error...\n";
            res = 0;
        }

        if (res != 0 && rres == 0) {
            rres = res;
        }
 
        if (curarg < args.size() && args[curarg] == ";" ) {
            curarg++;
            keep_going = 1;
        } else {
            keep_going = 0;
        }
    }

     for( curarg = 0 ; curarg < args.size(); curarg++ ) {
	if (0 != (sbp =  cache_stat(args[curarg].c_str()))) {
	    if (curarg == args.size() - 1 || args[curarg+1] == ";" ) {
		dstsize += sbp->st_size;
	    } else {
		srcsize += sbp->st_size;
	    }
	}
     }

    gettimeofday(&time_after,0);

    if (cleanup_stage) {
        _debug && std::cerr << "removing: " << args[curarg - 2 ] << endl;
        unlink( args[curarg - 2].c_str());
    }

    if (need_copyback) {
        res =  system("ifdh_copyback.sh");
        if (WIFSIGNALED(res)) throw( std::logic_error("signalled while running copyback"));
    }

    // only report statistics if the copy succeeded!
    if (rres == 0) {

    long int copysize;
    stringstream logmessage;
    // if we didn't get numbers from getrusage, try the sums of
    // the stat() st_size values for in and out.
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
    this->log(logmessage.str());

    }

    if (rres != 0) {
        cerr << "ifdh cp failed at: " << ctime(&time_after.tv_sec) << endl;
    }
   
    if (need_cpn_lock) {
        cpn.free();
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
            if (0 == access(s.c_str(), W_OK)) {
                _debug && std::cerr << "unlinking: " << s << endl;
                unlink(s.c_str());
            } else {
                string srmcmd("srmrm -2 ");
                srmcmd +=  bestman_srm_uri + s + " ";
                _debug && std::cerr << "running: " << srmcmd << endl;
                res = system(srmcmd.c_str());
                if (WIFSIGNALED(res)) throw( std::logic_error("signalled while cleaning stage directories"));
            }
        }
    }
    return res;
}

void
pick_type( string &loc, string force, bool &use_fs, bool &use_gridftp, bool &use_srm, bool &use_irods, bool &use_s3) {

    if (force.length() == 0) {
       if (getenv("IFDH_FORCE")) {
          force += "--force=";
          force += getenv("IFDH_FORCE");
       }
    }
    if (force.find("--force=") == 0L) {
        ifdh::_debug && std::cerr << "force type: " << force[8] << endl;
        switch(force[8]) {
        case 'e': case 'g': use_gridftp = true; break;
        case 's':
                 if (force[9] == '3') {
                            use_s3 = true;     
                 } else {
                            use_srm = true;
                 }
                break;
        case 'i':           use_irods = true;     break;
        default:
        case 'c': case 'd': use_fs = true;      break;
        }
    }

    if (loc.find(':') > 1 && loc.find(':') != string::npos) {
        if (loc.find("s3:") == 0) {
           use_s3 = true;
        }
        if (loc.find("i:") == 0) {
           use_irods = true;
        }
        if (loc.find("srm:") == 0) {
           use_srm = true;
        }
        if (loc.find("gsiftp:") == 0) {
           use_gridftp = true;
        }
        if (loc.find("file:") == 0) {
           use_fs = true;
           loc = loc.substr(8);
        }
    } else {
        // no xyz: on front...
        if ( loc.find("/pnfs") == 0 ) {
            use_gridftp = !use_srm;
            loc = map_pnfs(loc, use_srm);

        } else if( use_gridftp )  {
            if (force.find("--force=") == 0L && force[8] == 'e') {
                std::string gftpHost("gsiftp://if-gridftp-");
                gftpHost.append(getexperiment());
                gftpHost.append(".fnal.gov");
                loc = gftpHost + loc;
            } else {
                loc = bestman_ftp_uri + loc;
            }
            ifdh::_debug && std::cerr << "use_gridftp converting to: " << loc << endl;
        } else if( use_srm )  {
            loc = bestman_srm_uri + loc;
            ifdh::_debug && std::cerr << "use_srm converting to: " << loc << endl;
        }

    }

    if (!(use_fs || use_gridftp || use_srm || use_irods || use_s3 )) {

        if (loc[0] != '/') {
           string cwd(get_current_dir_name());
           loc = cwd + '/' + loc;
         }

        // if it's not visible, it's assumed to be either
        // dcache or bluearc...
        int r1 =  access(parent_dir(loc).c_str(), R_OK);
        ifdh::_debug && std::cerr << "ifdh ls: local_access returns " << r1 <<"\n";
        
        if (0 != r1 ) {
             
            use_gridftp = true;
            if ( loc.find("/pnfs") == 0 ) {
                loc = map_pnfs(loc, 0);
            } else {
                std::string gftpHost("gsiftp://if-gridftp-");
                gftpHost.append(getexperiment());
                gftpHost.append(".fnal.gov");
                loc = gftpHost + loc;
            }
        } else {
            use_fs = true;
        }
    }
    // not really part of picking, but everyone did it right afterwards,
    // so just putting it in one place instead.
    if (use_srm || use_gridftp || use_irods) {
        get_grid_credentials_if_needed();
    }
}

vector<string>
ifdh::ls(string loc, int recursion_depth, string force) {
    vector<string> res;
    std::vector<std::pair<std::string,long> > llout;

    // return just the names from lss's output
    llout = this->lss(loc, recursion_depth, force );
    for(size_t i = 0; i < llout.size(); i++) { 
       res.push_back(llout[i].first);
    }
    return res;
}

int
ifdh::ll( std::string loc, int recursion_depth, std::string force) {

    std::vector<std::pair<std::string,long> >  res;

    bool use_gridftp = false;
    bool use_srm = false;
    bool use_fs = false;
    bool use_irods = false;
    bool use_s3 = false;
    std::stringstream cmd;

    if ( -1 == recursion_depth )
        recursion_depth = 1;

    pick_type( loc, force, use_fs, use_gridftp, use_srm, use_irods, use_s3);


    if (use_srm) {
       setenv("SRM_JAVA_OPTIONS", "-Xmx1024m" ,0);
       cmd << "srmls -l -2 -count=8192 ";
       cmd << "--recursion_depth " << recursion_depth << " ";
       cmd << loc;
    } else if (use_s3) {
       cmd << "aws s3 ls -l ";
       if (recursion_depth > 1) {
           cmd << "-r ";
       }
       cmd << loc;
    } else if (use_irods) {
       cmd << "ils -l ";
       if (recursion_depth > 1) {
           cmd << "-r ";
       }
       cmd << loc;
    } else if (use_gridftp) {
       cmd << "uberftp -ls ";
       if (recursion_depth > 1) {
           cmd << "-r ";
       }
       cmd << loc;
    } else if (use_fs) {
       // find uses an off by one depth from srmls
       cmd << "find " << loc << 
           " -maxdepth " << recursion_depth << 
          " \\( -type d -ls \\)  " <<
           " " ;
    }

    _debug && std::cerr << "ifdh ll: running: " << cmd.str() << endl;

    int status = system(cmd.str().c_str());
    if (WIFSIGNALED(status)) throw( std::logic_error("signalled while doing ll"));
    return WEXITSTATUS(status);
}

std::vector<std::pair<std::string,long> > 
ifdh::lss( std::string loc, int recursion_depth, std::string force) {

    std::vector<std::pair<std::string,long> >  res;

    bool use_gridftp = false;
    bool use_srm = false;
    bool use_fs = false;
    bool use_irods = false;
    bool use_s3 = false;
    bool first = true;
    std::stringstream cmd;
    std::string dir, base, dir_last;
    size_t cpos, spos, qpos, pos, fpos;

    if ( -1 == recursion_depth )
        recursion_depth = 1;

    cpos = loc.find(':');
    if (cpos > 1 && cpos < 8) {
       // looks like a uri...
       spos = loc.find('/',cpos+3);
       if( spos != string::npos) {
          qpos = loc.find("?SFN=",spos);
          if (qpos != string::npos) {
              base = loc.substr(0,qpos+5);
          } else {
              base = loc.substr(0,spos);
          }
       }
    }
    if(_debug) std::cerr << "came up with base:" << base << endl;

    pick_type( loc, force, use_fs, use_gridftp, use_srm, use_irods, use_s3);

    if (use_srm) {
       setenv("SRM_JAVA_OPTIONS", "-Xmx1024m" ,0);
       cmd << "srmls -2 -count=8192 ";
       cmd << "--recursion_depth " << recursion_depth << " ";
       cmd << loc;
    } else if (use_s3) {
       cmd << "aws s3 ls  ";
       if (recursion_depth > 1) {
           cmd << "--recursive ";
       }
       cmd << loc;
       dir = loc.substr(loc.find("/",5));
       base = base + dir;
    } else if (use_irods) {
       cmd << "ils  ";
       if (recursion_depth > 1) {
           cmd << "-r ";
       }
       cmd << loc;
       dir = loc.substr(loc.find("/",4));
    } else if (use_gridftp) {
       cmd << "uberftp -ls ";
       if (recursion_depth > 1) {
           cmd << "-r ";
       }
       cmd << loc;
       dir = loc.substr(loc.find("/",9));
    } else if (use_fs) {
       // find uses an off by one depth from srmls
       cmd << "find " << loc << 
           " -maxdepth " << recursion_depth << 
          " \\( -type d -printf '%s %p/\\n' -o -printf '%s %p\\n' \\)  " <<
           " " ;
    }

    spos = dir.rfind('/');
    if (spos != string::npos) {
      dir_last = dir.substr(spos+1);
    }
    _debug && std::cerr << "ifdh ls: running: " << cmd.str() << endl;

    FILE *pf = popen(cmd.str().c_str(), "r");
    char buf[512];
    long fsize;
    while (!feof(pf) && !ferror(pf)) {
	if (fgets(buf, 512, pf)) {
           string s(buf);
           _debug && std::cerr << "before cleanup: |" << s <<  "|\n";
           fsize = 0;
           // trim trailing newlines
           if ('\n' == s[s.size()-1]) {
               s = s.substr(0,s.size()-1);
           }
           if ('\r' == s[s.size()-1]) {
               s = s.substr(0,s.size()-1);
           }

           // if the first thing we see ends in the last component
           // of the name of our base directory, assume we got asked
           // to ls /d1/d2/d3/file and we see 'file'
           // otherwise we would see /d1/d2/d3/d4/ (trailing slash) 
           // if it is a directory...
           // Anhow if we see this, trim a component off of base so
           // we dont make it /d1/d2/d3/file/file when we list it.
           if (first) {
               first = false;
               if (dir_last.size()) {
                   pos = s.rfind(dir_last);
                   if (pos != string::npos && pos + dir_last.size() == s.size()) {
                       dir = dir.substr(0,dir.rfind('/'));
                   }
               }
           }
           // trim leading stuff from srmls/aws s3 ls
           if (use_srm || use_s3) {
               // find spos as start of size , which is after date
               // on s3
               spos = s.find_first_of("0123456789", use_s3?20:0);
	       pos = s.find('/');
               if (spos != string::npos && spos < pos) {
                   // we have digits before the path...
                   fsize = atol(s.c_str()+spos);
                   pos = s.find(" ", spos);
                   pos = pos + 1;
               }
	       if (pos > 0 && pos != string::npos ) {
		   s = s.substr(pos);
	       }
               if (s == "")
                   continue;
           }
           if (use_gridftp) {
               // trim long listing bits, (8 columns) add slash if dir
                 
               // find flags (i.e drwxr-xr-x...)
               fpos = s.find_first_of("drwsx-");   		if (fpos == string::npos) continue;
               if (_debug) cerr << "fpos is" << fpos << endl;
               // find path
               pos = s.rfind(' ');				if (pos == string::npos) continue;
               pos = pos + 1;
               // find space column -- skip back over date
               // look backwards for a space, then forwards for a digit
               spos = pos - 16;
               spos = s.rfind(' ', spos);		 	if (spos == string::npos) continue;
               spos = s.find_first_of("0123456789",spos);	if (spos == string::npos) continue;
               if (_debug) cerr << "spos is " << spos << endl;
               fsize = atol(s.c_str()+spos);	
               if (_debug) cerr << "fsize is " << fsize << endl;
               if (_debug) cerr << "pos is" << pos << endl;
               if (s[fpos] == 'd') {
                  s = s.substr(pos) + "/";
                  ifdh::_debug && std::cerr << "directory: " << s << endl;
               } else {
                  s = s.substr(pos);
               }
               // only gridftp lists . and ..
               if (s == "../" || s == "./" )
                   continue;
               if (s[0] != '/') {
                   s = dir + '/' +  s;
               }
           } 
           if (use_fs) {
               fsize = atol(s.c_str());
               pos = s.find('/');				if (pos == string::npos) continue;
	       s = s.substr(pos);
               
           }
           s = base + s;
           res.push_back(pair<string,long>(s,fsize));
        }
    }
    int status = pclose(pf);
    if (WIFSIGNALED(status)) throw( std::logic_error("signalled while doing ls"));
    return res;
}
   
int
ifdh::mkdir(string loc, string force) {
    bool use_gridftp = false;
    bool use_srm = false;
    bool use_fs = false;
    bool use_irods = false;
    bool use_s3 = false;
    std::stringstream cmd;

    pick_type( loc, force, use_fs, use_gridftp, use_srm, use_irods, use_s3);

    if (use_fs)      cmd << "mkdir ";
    if (use_gridftp) cmd << "uberftp -mkdir ";
    if (use_srm)     cmd << "srmmkdir -2 ";
    if (use_irods)   cmd << "imkdir ";
    if (use_s3)      cmd << "aws s3 mkdir ";

    cmd << loc;

    _debug && std::cerr << "running: " << cmd.str() << endl;

    int status = system(cmd.str().c_str());
    if (WIFSIGNALED(status)) throw( std::logic_error("signalled while doing mkdir"));
    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) throw( std::logic_error("mkdir failed"));
    return 0;
}

int
ifdh::rm(string loc, string force) {
    bool use_gridftp = false;
    bool use_srm = false;
    bool use_fs = false;
    bool use_irods = false;
    bool use_s3 = false;
    std::stringstream cmd;

    pick_type( loc, force, use_fs, use_gridftp, use_srm, use_irods, use_s3);

    if (use_fs)      cmd << "rm ";
    if (use_gridftp) cmd << "uberftp -rm ";
    if (use_srm)     cmd << "srmrm -2 ";
    if (use_irods)   cmd << "irm ";
    if (use_s3)   cmd << "aws s3 rm ";

    cmd << loc;

    _debug && std::cerr << "running: " << cmd.str() << endl;

    int status = system(cmd.str().c_str());
    if (WIFSIGNALED(status)) throw( std::logic_error("signalled while doing rm"));
    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) throw( std::logic_error("rm failed"));
    return 0;
}

int
ifdh::rmdir(string loc, string force) {
    bool use_gridftp = false;
    bool use_srm = false;
    bool use_fs = false;
    bool use_irods = false;
    bool use_s3 = false;
    std::stringstream cmd;

    pick_type( loc, force, use_fs, use_gridftp, use_srm, use_irods, use_s3);

    if (use_fs)      cmd << "rmdir ";
    if (use_gridftp) cmd << "uberftp -rmdir ";
    if (use_srm)     cmd << "srmrmdir -2 ";
    if (use_irods)   cmd << "irm ";
    if (use_s3)   cmd << "aws s3 rm ";

    cmd << loc;

    _debug && std::cerr << "running: " << cmd.str() << endl;

    int status = system(cmd.str().c_str());
    if (WIFSIGNALED(status)) throw( std::logic_error("signalled while doing rmdir"));
    // if (WIFEXITED(status) && WEXITSTATUS(status) != 0) throw( std::logic_error("rmdir failed"));
    return WEXITSTATUS(status);
}

std::string
mbits( int n ) {
    std::stringstream res;
    if ( n & 04 ) 
       res << 'R';
    if ( n & 02 ) 
       res << 'W';
    if ( n & 01 ) 
       res << 'X';
    return res.str();
}

int
ifdh::chmod(string mode, string loc, string force) {
    bool use_gridftp = false;
    bool use_srm = false;
    bool use_fs = false;
    bool use_irods = false;
    bool use_s3 = false;
    std::stringstream cmd;

    pick_type( loc, force, use_fs, use_gridftp, use_srm, use_irods, use_s3);

    if (use_fs)      cmd << "chmod ";
    if (use_gridftp) cmd << "uberftp -chmod ";
    // commented out srm bits here are an alternate possiblity --
    // using srm-permission-set versus srm-set-permissions(!)
    // -- note srm-set-permissions gives errors about ACL support
    //    from DCache SRM. So we don't do that now.
    //if (use_srm)     cmd << "srm-permission-set ";
    if (use_srm)     cmd << "srm-set-permissions ";
    if (use_irods)   cmd << "ichmod ";
    if (use_s3)   cmd << "aws s3 chmod "; // does this actually exist?

    if (use_srm) {
       int imode = strtol(mode.c_str(), 0, 8);
       cmd << "-type=CHANGE ";
       cmd << "-owner=" << mbits((imode >> 6) & 7) << " ";
       cmd << "-group=" << mbits((imode >> 3) & 7) << " ";
       cmd << "-other=" << mbits((imode >> 0) & 7) << " ";
    } else {
       cmd << mode << " ";
    }

    cmd << loc;

    _debug && std::cerr << "running: " << cmd.str() << endl;

    int status = system(cmd.str().c_str());
    if (WIFSIGNALED(status)) throw( std::logic_error("signalled while doing chmod"));
    // if (WIFEXITED(status) && WEXITSTATUS(status) != 0) throw( std::logic_error("chmod failed"));
    return WEXITSTATUS(status);
}

int
ifdh::pin(string loc, long int secs) {
    bool use_gridftp = false;
    bool use_srm = false;
    bool use_fs = false;
    bool use_irods = false;
    bool use_s3 = false;
    std::stringstream cmd;

    // if it is not pnfs, ignore it.
    if (loc.find("/pnfs") == string::npos) {
        _debug && std::cerr << "skiping pin of non pnfs " << loc << endl;
        return  0;
    }

    // we have to do this via srm for the moment
    pick_type( loc, "--force=srm", use_fs, use_gridftp, use_srm, use_irods, use_s3);

    cmd << "srm-bring-online --lifetime=" << secs << " " << loc;
    _debug && std::cerr << "running: " << cmd.str() << endl;

    int status = system(cmd.str().c_str());
    if (WIFSIGNALED(status)) throw( std::logic_error("signalled while doing rmdir"));
    // if (WIFEXITED(status) && WEXITSTATUS(status) != 0) throw( std::logic_error("rmdir failed"));
    return WEXITSTATUS(status);
}
       
int 
ifdh::rename(string loc, string loc2, string force) {
    bool use_gridftp = false;
    bool use_srm = false;
    bool use_fs = false;
    bool use_irods = false;
    bool use_s3 = false;
    stringstream cmd;

    pick_type( loc, force, use_fs, use_gridftp, use_srm, use_irods, use_s3);
    pick_type( loc2, force, use_fs, use_gridftp, use_srm, use_irods, use_s3);

    if (use_fs)      cmd << "mv ";
    if (use_gridftp) cmd << "uberftp -rename ";
    if (use_srm)     cmd << "srmmv ";
    if (use_irods)   cmd << "imv ";
    if (use_s3)      cmd << "aws s3 mv ";

    // uberftp doesn't want the second argument to be a full uri, 
    // but rather a path.
    if (use_gridftp) {
         size_t slpos = loc2.find('/',9);
         loc2 = loc2.substr(slpos);
    }

    cmd << loc << " " << loc2;

    _debug && cerr << "running: " << cmd.str() << endl;

    int status = system(cmd.str().c_str());
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
       if (dlist1[i] == "srm" || dlist1[i] == "gsiftp" || dlist1[i] == "http") {
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
        batch = this->lss(dlist[i],10,"");
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
   string rdpath("root://fndca.fnal.gov:1094/pnfs/fnal.gov");

   if (_debug) cerr << "fetchShared -- schema is " << schema << endl;

   for(size_t i = 0; i < list.size(); i++ ) {
       if (_debug) cerr << "fetchShared item " << i << " " << list[i].first << endl;
       // for now, if we found it via a cvmfs path, return it, else
       // run it through ifdh::fetchInput to get a local path
       if (list[i].first.find("/cvmfs/") == 0 ) {
           f = list[i].first;
       } else {
           if (schema == "xrootd" && list[i].first.find("/pnfs") == 0) {
               f = rdpath + list[i].first;
           } else {
               f = this->fetchInput( list[i].first );
           }
       }
       res.push_back( pair<string,long>(f, list[i].second));
   }
   return res;
}

}
