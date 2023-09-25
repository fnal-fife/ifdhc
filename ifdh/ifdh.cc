#include "ifdh.h"
#include "ifdh_mbuf.h"
#include "utils.h"
#include <fcntl.h>
#include <string>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <../numsg/numsg.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <exception>
#include <sys/wait.h>
#include <signal.h>
#include <sys/signal.h>
#include <sys/param.h>
#include <map>
#include "../util/Checksum.h"
#include <../util/regwrap.h>
#include <setjmp.h>
#include <memory>
#ifndef __APPLE_CC__
#include <gnu/libc-version.h>
#endif
#include <uuid/uuid.h>

time_t gt;

#if __cplusplus <= 199711L
#define unique_ptr auto_ptr
#endif

using namespace std;

namespace ifdh_ns {
 
int ifdh::_debug;
WimpyConfigParser ifdh::_config;
std::string ifdh::_config_version;


void
path_prepend( string s1, string s2) {
    stringstream setenvbuf;
    string curpath(getenv("PATH")?getenv("PATH"):"/bin:/usr/bin");

    setenvbuf << s1 << s2 << ":" << curpath;
    setenv("PATH", setenvbuf.str().c_str(), 1);
}

void
path_ish_append(const char *what, string s1, string s2) {
    stringstream setenvbuf;
    string curpath;
    if (getenv(what)) {
        curpath = getenv(what);
     } else {
        curpath="";
     }

    setenvbuf << curpath << ":" << s1 << s2;
    setenv(what, setenvbuf.str().c_str(), 1);
}


void
check_env() {
    static int checked = 0;
    static const char *cpn_basedir = "/grid/fermiapp/products/common/prd/cpn";

    if (!checked) {
        checked = 1;

        srandom(getpid()*getuid());

        // try to find cpn even if it isn't setup
        if (!getenv("CPN_DIR")) {
           if (0 == access(cpn_basedir, R_OK)) {
              for (int i = 10; i > 0; i-- ) {
                  stringstream setenvbuf;
                  setenvbuf << cpn_basedir << "/v1_" << i << "/NULL";
                  if (0 == access(setenvbuf.str().c_str(), R_OK)) {
                      setenv("CPN_DIR", setenvbuf.str().c_str(), 1);
                      break;
                  }
              }
           }
        }

        // we do not want it set...
        unsetenv("X509_USER_CERT");

        char *ep;

        if (0 != (ep = getenv("EXPERIMENT")) && 0 == getenv("CPN_LOCK_GROUP")) {
            setenv("CPN_LOCK_GROUP", ep, 0);
        }
              
        string path(getenv("PATH")?getenv("PATH"):"");
        char *p;

        if (0 != (p = getenv("VDT_LOCATION"))) {
            if (string::npos == path.find(p)) {
               path_prepend(p,"/globus/bin");
               path_prepend(p,"/srm-client-fermi/bin");
            }
        } else if (0 == access("/usr/bin/globus-url-copy", R_OK)) {
            if (string::npos == path.find("/usr/bin")) {
               path_prepend("/usr","/bin");
            }
        } else {
             // where is vdt?!?
             ;
        }
    }

#ifdef _GNU_LIBC_VERSION_H

    // put cmvfs OSG utils at the end of our path as a failover/fallback
    // currently assuming 64bit is okay
    int glibcminor = atoi(gnu_get_libc_version()+2);
    int slver = glibcminor > 5 ? (glibcminor > 12 ? 7 : 6) : 5;
    path_ish_append("PATH","/usr/bin","");
    path_ish_append("LD_LIBRARY_PATH","/usr/lib64","");
    stringstream cvmfs_dir, ocvmfs_dir;
    cvmfs_dir << "/cvmfs/oasis.opensciencegrid.org/mis/osg-wn-client/3.6/current/el" <<  slver << "-x86_64";
    ocvmfs_dir << "/cvmfs/oasis.opensciencegrid.org/mis/osg-wn-client/3.5/current/el" <<  slver << "-x86_64";
    path_ish_append("PATH",cvmfs_dir.str().c_str(),"/usr/bin");
    path_ish_append("PATH",ocvmfs_dir.str().c_str(),"/usr/bin");
    path_ish_append("LD_LIBRARY_PATH",cvmfs_dir.str().c_str(),"/usr/lib64");
#endif
}

ifdh::ifdh(std::string baseuri) {

    check_env();
    char *debug = getenv("IFDH_DEBUG");
    if (0 != debug ) { 
	_debug = atoi(debug);
        _debug && std::cerr << "IFDH_DEBUG=" << debug << " => " << atoi(debug) << "\n";
    }
    if ( baseuri == "" ) {
        _baseuri = getenv("IFDH_BASE_URI")?getenv("IFDH_BASE_URI"):(getenv("EXPERIMENT")?_default_base_uri+getenv("EXPERIMENT")+"/api":"") ;
     } else {
       _baseuri = baseuri;
    }
    _debug && std::cerr << "ifdh constructor: _baseuri is '" << _baseuri << "'\n";
    
    // -------------------------------------------------------
    // parse and initialize config file
    //  
    if (_config.size() == 0) {
       _debug && std::cerr << "checking configs: \n";
       _config.getdefault( getenv("IFDHC_CONFIG_DIR"), getenv("IFDHC_DIR"), getenv("IFDHC_FQ_DIR"),_debug);
       if ( _config.get("general","protocols") == "" ||  _config.get("general","prefixes") == "") {
           throw(std::logic_error("ifdh: bad config file: no protocols or prefixes"));
       }
       if ( _config.get("general","version") != "") {
           _config_version = _config.get("general","version");
       }
       _debug && std::cerr << "ifdh: config file version is " << _config_version << "\n";
    }
    _dd_mc_session_tok.clear();
}


void
ifdh::set_base_uri(std::string baseuri) { 
    _baseuri = baseuri; 
}


std::string
ifdh::unique_string() {
    uuid_t uuid;
    char uuidbuf[80];
    uuid_generate(uuid);
    uuid_unparse(uuid,uuidbuf);
    return std::string(uuidbuf);
}
string cpn_loc  = "cpn";  // just use the one in the PATH -- its a product now
string fermi_gsiftp  = "gsiftp://fg-bestman1.fnal.gov:2811";
string bestmanuri = "srm://fg-bestman1.fnal.gov:10443/srm/v2/server?SFN=";



string datadir() {
    stringstream dirmaker;
    string localpath;
    int res;
    int pgrp, pid;
    int useid;

    pid = getpid();
    pgrp = getpgrp();

    // if we are our own proccess group, we assume our
    // parent is a job-control shell, and use it's pid.
    // otherwise use process group.
    if (pgrp == pid) {
       useid = getppid();
    } else {
       useid = pgrp;
    }
    
    if (getenv("IFDH_DATA_DIR")) {
       dirmaker << getenv("IFDH_DATA_DIR");
    } else { 
	dirmaker << (
	   getenv("_CONDOR_SCRATCH_DIR")?getenv("_CONDOR_SCRATCH_DIR"):
	   getenv("TMPDIR")?getenv("TMPDIR"):
           "/var/tmp"
        )
       << "/ifdh_" << getuid() << "_" << useid;
    }

    if ( 0 != access(dirmaker.str().c_str(), W_OK) ) {
        res = mkdir(dirmaker.str().c_str(),0700);
        ifdh::_debug && cerr <<  "mkdir " << dirmaker.str() << " => " << res << "\n";
    }
    localpath = dirmaker.str();
    if (localpath.substr(0,2) == "//" ){ 
        localpath = localpath.substr(1);
    }
    return localpath;
}


std::string 
ifdh::getErrorText() {
   return _errortxt;
}

int
ifdh::cleanup() {
    int res;
    string cmd("rm -rf ");
    cmd = cmd + datadir();
    res = system(cmd.c_str());
    if (WIFSIGNALED(res)) throw( std::logic_error("signalled while removing cleanup files"));
    return WEXITSTATUS(res);
}
// file io

// extern "C" { const char *get_current_dir_name(); }

string 
ifdh::localPath( string src_uri ) {
    int baseloc = src_uri.rfind("/") + 1;
    return datadir() + "/" + src_uri.substr(baseloc);
}



string 
ifdh::fetchInput( string src_uri ) {
    stringstream cmd;
    stringstream err;
    string path;

    // be prepared for failure
    std::string error_message("Copy Failed for: ");
    error_message += src_uri;

    std::vector<std::string> args;

    if (0 == src_uri.find("xroot:") || 0 == src_uri.find("root:")) {
        char *icx = getenv("IFDH_COPY_XROOTD");
        char *ipx = getenv("IFDH_PASS_XROOTD");
        ifdh_op_msg mbuf("cp", *this);
        // copy it anyway if IFDH_COPY_XROOTD is set or if the file
        // does *not* end in .root, as the app won't be able to stream
        // it anyway, unless IFDH_PASS_XROOTD is set since the user knows best.
        _debug && std::cerr << "Checking for IFDH_COPY_XROOTD, IFDH_PASS_XROOTD, or suffix:" << src_uri.substr(src_uri.length()-5,5) << "\n";
        if ((ipx && atoi(ipx)) && (icx && atoi(icx))) {
            std::cerr << "Warning: found both IFDH_COPY_XROOTD and IFDH_PASS_XROOTD; you must know what you're doing, passing through\n";
        }
        mbuf.src = src_uri;
        mbuf.dst = path;
        if (!(ipx && atoi(ipx)) && ((icx && atoi(icx)) || (src_uri.substr(src_uri.length()-5,5) != ".root"))) {
             path = localPath( src_uri );
             args.push_back(src_uri);
             args.push_back(path);
             if ( 0 == cp( args ) && flushdir(datadir().c_str()) && 0 == access(path.c_str(),R_OK)) {
                 const char *pc_buf;
                 extern void rotate_door(WimpyConfigParser &_config, const char *&cmd_str, std::string &cmd_str_string, ifdh_op_msg &mbuf);
                 pc_buf = path.c_str();
                 rotate_door(_config, pc_buf, path, mbuf);
                 _lastinput = path;
                 mbuf.log_success();
                 return path;
             } else {
                 mbuf.log_failure();
                 return "";
             }
        } else {
            // we don't do anything for xrootd, just pass
            // it back, and let the application layer open
            // it that way.
            return src_uri;
        }
    }
    path = localPath( src_uri );
    if (0 == src_uri.find("file:///"))
       args.push_back(src_uri.substr(7));
    else
       args.push_back(src_uri);
    args.push_back(path);
    _debug && std::cerr << "args: " << args[0] << ", " << args[1] << "\n";
    try {
       if ( 0 == cp( args ) && flushdir(datadir().c_str()) && 0 == access(path.c_str(),R_OK)) {
          _lastinput = path;
          return path;
       } else {
         throw( std::logic_error("see error output"));
       }
    } catch( exception &e )  {
       // std::cerr << "fetchInput: Exception: " << e.what();
       error_message += ": exception: ";
       error_message += e.what();
       throw( std::logic_error(error_message));
    }
}

// file output
//
// keep track of output filenames and last input file name
// for renameOutput, and copytBackOutput...
//
int 
ifdh::addOutputFile(string filename) {
    size_t pos;

    if (0 != access(filename.c_str(), R_OK)) {
         throw( std::logic_error((filename + ": file does not exist!").c_str()));
    }
    bool already_added = false;
    try {
        string line;
        fstream outlog_in((datadir()+"/output_files").c_str(), ios_base::in);

        while (!outlog_in.eof() && !outlog_in.fail()) {
           getline(outlog_in, line);
           if (line.substr(0,line.find(' ')) == filename) {
               already_added = true;
           }
        }
        outlog_in.close();
    } catch (exception &e) {
       ;
    }
    if (already_added) {
        throw( std::logic_error((filename + ": added twice as output file").c_str()));
    }
    fstream outlog((datadir()+"/output_files").c_str(), ios_base::app|ios_base::out);
    if (!_lastinput.size() && getenv("IFDH_INPUT_FILE")) {
        _lastinput = getenv("IFDH_INPUT_FILE");
    }
    pos = _lastinput.rfind('/');
    if (pos != string::npos) {
        _lastinput = _lastinput.substr(pos+1);
    }
    outlog << filename << " " << _lastinput << "\n";
    outlog.close();
    return 1;
}

#include "md5.h"
#include "../util/sha256.h"

std::string
get_hashdir(std::string filename, int n) {
    std::stringstream hashdir;
    unsigned char digest[32];

    char *algenv = getenv("IFDH_DIR_HASH_ALG");
    if (algenv && 0 == strcmp(algenv, "sha256")) {
        SHA256_CTX sha256;
        SHA256_Init(&sha256);
        SHA256_Update(&sha256, filename.c_str(), filename.size());
        SHA256_Final(digest, &sha256);
    } else { 
        md5_state_t ms;
        md5_init(&ms);
        md5_append(&ms, (const md5_byte_t*)filename.c_str(), filename.size());
        md5_finish(&ms, digest);
    }

    for (int i = 0 ; i< n; i++ ) {
        hashdir << '/';
        hashdir << std::hex << setw(2) << setfill('0');
        hashdir << (unsigned int)(digest[i]);
    }

    return hashdir.str();
}

int 
ifdh::copyBackOutput(string dest_dir, int hash) {
    string line;
    string file;
    string filelast;
    string sep(";");
    bool first = true;
    vector<string> cpargs;
    size_t spos;
    string outfiles_name;
    // int baseloc = dest_dir.find("/") + 1;
    outfiles_name = datadir()+"/output_files";
    fstream outlog(outfiles_name.c_str(), ios_base::in);
    if ( outlog.fail()) {
       return 0;
    }

    while (!outlog.eof() && !outlog.fail()) {

        getline(outlog, line);

        _debug && std::cerr << "parsing: |" << line << "|\n";
	spos = line.find(' ');
	if (spos != string::npos) {
	    file = line.substr(0,spos);
	} else {
	    file = line;
	}
	if ( !file.size() ) {
	   break;
	}
	spos = file.rfind('/');
	if ( spos != string::npos ) {
	    filelast = file.substr(spos+1);
	} else {
	    filelast = file;
	}
        if (!first) {
            cpargs.push_back(sep);
        }
        first = false;
        cpargs.push_back(file);
        std::string tdest;
        if (hash) {
            tdest = dest_dir + get_hashdir(filelast, hash);
            mkdir_p(tdest,"",hash+1);
        } else {
            tdest= dest_dir;
        }
        cpargs.push_back(tdest + "/" + filelast);
        _debug && std::cerr << "adding cp of " << file << " " << tdest << "/" << filelast << "\n";
   
    }
    return cp(cpargs);
}

// logging
int 
ifdh::log( string message ) {

  if (!numsg::getMsg()) {
      const char *user;
      (user = getenv("GRID_USER")) || (user = getenv("USER"))|| (user = "unknown");
      string idstring(user);
      idstring += "/";
      idstring += getexperiment();
      if (getenv("POMS_TASK_ID")) {
          idstring += ':';
          idstring += getenv("POMS_TASK_ID");
      }
      if (getenv("JOBSUBJOBID")) {
          idstring += '/';
          idstring += getenv("JOBSUBJOBID");
      } else {
	  if (getenv("CLUSTER")) {
	      idstring += '/';
	      idstring += getenv("CLUSTER");
	  }
	  if (getenv("PROCESS")) {
	      idstring += '.';
	      idstring += getenv("PROCESS");
	  }
      }
      numsg::init(idstring.c_str(),0);
  }
  numsg::getMsg()->printf("ifdh: %s", message.c_str());
  return 0;
}

int 
ifdh::enterState( string state ){
  numsg::getMsg()->start(state.c_str());
  return 1;
}

int 
ifdh::leaveState( string state ){
  numsg::getMsg()->finish(state.c_str());
  return 1;
}


}

