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

int
ifdh::_debug = 0;

WimpyConfigParser
ifdh::_config;


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

//
// make sure appropriate environment stuff is set
// and improve our chances of finding cpn, globus tools, etc.
//
static void
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
    stringstream cvmfs_dir;
    cvmfs_dir << "/cvmfs/oasis.opensciencegrid.org/mis/osg-wn-client/3.3/current/el" <<  slver << "-x86_64";
    path_ish_append("PATH",cvmfs_dir.str().c_str(),"/usr/bin");
    path_ish_append("LD_LIBRARY_PATH",cvmfs_dir.str().c_str(),"/usr/lib64");
#endif
}

string cpn_loc  = "cpn";  // just use the one in the PATH -- its a product now
string fermi_gsiftp  = "gsiftp://fg-bestman1.fnal.gov:2811";
string bestmanuri = "srm://fg-bestman1.fnal.gov:10443/srm/v2/server?SFN=";

std::string ifdh::_default_base_ssl_uri = "https://samweb.fnal.gov:8483/sam/";
std::string ifdh::_default_base_uri =  ifdh::_default_base_ssl_uri;

string ssl_uri(string s) {
   // it is all ssl now...
   return s;
}

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
        ifdh_op_msg mbuf("cp", *this);
        // copy it anyway if IFDH_COPY_XROOTD is set or if the file
        // does *not* end in .root, as the app won't be able to stream
        // it anyway.
        _debug && std::cerr << "Checking for IFDH_COPY_XROOTD or suffix:" << src_uri.substr(src_uri.length()-5,5) << "\n";
        mbuf.src = src_uri;
        mbuf.dst = path;
        if ((icx && atoi(icx)) || (src_uri.substr(src_uri.length()-5,5) != ".root")) {
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

WebAPI * 
do_url_2(int postflag, va_list ap) {
    stringstream url;
    stringstream postdata;
    static string urls;
    static string postdatas;
    const char *sep = "";
    char * name;
    char * val;

    int count = 0;
    val = va_arg(ap,char *);
    if (val == 0 || *val == 0) {
        throw(WebAPIException("Environment variables IFDH_BASE_URI and EXPERIMENT not set and no URL set!",""));
    }
    while (strlen(val)) {
 
        url << sep << (count++ ? WebAPI::encode(val) : val);
        val = va_arg(ap,char *);
        sep = "/";
    }

    sep = postflag? "" : "?";

    name = va_arg(ap,char *);
    val = va_arg(ap,char *);
    while (strlen(name)) {
        if (strlen(val)) {
           (postflag ? postdata : url)  << sep << WebAPI::encode(name) << "=" << WebAPI::encode(val);
        } else {
           // handle single blob of data case, name with no value..
           (postflag ? postdata : url)  << sep << name;
        }
        sep = "&";
        name = va_arg(ap,char *);
        val = va_arg(ap,char *);
    }
    urls = url.str();
    postdatas= postdata.str();
    
    if (urls.find("https:") == 0) {
       extern void get_grid_credentials_if_needed();
       get_grid_credentials_if_needed();
    }

    if (ifdh::_debug) std::cerr << "calling WebAPI with url: " << urls << " and postdata: " << postdatas << "\n";

    return new WebAPI(urls, postflag, postdatas);
}


int
ifdh::do_url_int(int postflag, ...) {
    va_list ap;
    int res;

    va_start(ap, postflag);
    try {
       unique_ptr<WebAPI> wap(do_url_2(postflag, ap));
       res = wap->getStatus() - 200;
       if (res > 0 && res < 6) res = 0; // 201,202,.. is also success..
       if (res != 0 ) {
           string line;
           _errortxt = "";
	   while (!wap->data().eof() && !wap->data().fail()) {
	      getline(wap->data(), line);
              _errortxt = _errortxt + line;
           }
       }
    } catch( exception &e )  {
       _errortxt = e.what();
       res = 300;
    }
    if (ifdh::_debug) std::cerr << "got back int result: " << res << "\n";
    return res;
}


string
ifdh::do_url_str(int postflag,...) {
    va_list ap;
    string res("");
    string line;
    try {
	va_start(ap, postflag);
	unique_ptr<WebAPI> wap(do_url_2(postflag, ap));
	while (!wap->data().eof() && !wap->data().fail()) {
	    getline(wap->data(), line);
	    if (wap->data().eof()) {
		 res = res + line;
	    } else {
		 res = res + line + "\n";
	    }
	}
    } catch( exception &e )  {
       _errortxt = e.what();
       if (!getenv("IFDH_SILENT") || !atoi(getenv("IFDH_SILENT"))) {
           std::cerr << (time(&gt)?ctime(&gt):"") << " ";
           std::cerr << "Exception: " << _errortxt;
       }
       return "";
    }
    // strip trailing newline
    if ( res[res.length()] == '\n' ) {
        res = res.substr(0,res.length()-1);
    }
    if (ifdh::_debug) std::cerr << "got back string result: " << res << "\n";
    return res;
}

vector<string>
ifdh::do_url_lst(int postflag,...) {
    va_list ap;
    string line;
    vector<string> empty;
    vector<string> res;
    try {
	va_start(ap, postflag);
	unique_ptr<WebAPI> wap(do_url_2(postflag, ap));
	while (!wap->data().eof() && !wap->data().fail()) {
	    getline(wap->data(), line);
	    if (! (line == "" && wap->data().eof())) {
		res.push_back(line);
	    }
	}
    } catch( exception &e )  {
       _errortxt = e.what();
       return empty;
    }
    return res;
}

int 
ifdh::declareFile(string json_metadata) {
  // debug...
  WebAPI::_debug = 1;
  return do_url_int(2,ssl_uri(_baseuri).c_str(),"files","",json_metadata.c_str(),"","","");
}

int 
ifdh::modifyMetadata(string filename, string json_metadata) {
  // debug...
  WebAPI::_debug = 1;
  return do_url_int(2,ssl_uri(_baseuri).c_str(),"files","name",filename.c_str(),"metadata","",json_metadata.c_str(),"","","");
}

int
ifdh::addFileLocation(string filename, string location) {
 return do_url_int(1,ssl_uri(_baseuri).c_str(),"files","name",filename.c_str(),"locations","","add",location.c_str(),"","");
}

//datasets
int 
ifdh::createDefinition( string name, string dims, string user, string group) {
  return do_url_int(1,ssl_uri(_baseuri).c_str(),"createDefinition","","name",name.c_str(), "dims", dims.c_str(), "user", user.c_str(),"group", group.c_str(), "","");
}

int 
ifdh::deleteDefinition( string name) {
  return  do_url_int(1,ssl_uri(_baseuri).c_str(),"definitions","name", name.c_str(),"delete","","");
}

string 
ifdh::takeSnapshot( string name ) {
  return do_url_str(0,ssl_uri(_baseuri).c_str(),"definitions", "name", name.c_str(), "snapshot",  "","");
}

string 
ifdh::describeDefinition( string name) {
  return do_url_str(0,ssl_uri(_baseuri).c_str(),"definitions", "name", name.c_str(), "describe",  "","");
}

vector<string> 
ifdh::translateConstraints( string dims) {
  return do_url_lst(0,ssl_uri(_baseuri).c_str(),"files", "list", "", "dims", dims.c_str(), "","" );
}

// files
vector<string> 
ifdh::locateFile( string name, string schema ) {
  if (schema == "" ) {
      return do_url_lst(0,ssl_uri(_baseuri).c_str(), "files", "name", name.c_str(), "locations", "", "" );  
  } else {
      return do_url_lst(0,ssl_uri(_baseuri).c_str(), "files", "name", name.c_str(), "locations", "url", "" , "schema", schema.c_str(), "", "");
  }
}

string ifdh::getMetadata( string name) {
  return  do_url_str(0, ssl_uri(_baseuri).c_str(),"files","name", name.c_str(), "metadata","","");
}

//
string 
ifdh::dumpStation( string name, string what ) {
  
  if (name == "" && getenv("SAM_STATION"))
      name = getenv("SAM_STATION}");

  return do_url_str(0,ssl_uri(_baseuri).c_str(),"dumpStation", "", "station", name.c_str(), "dump", what.c_str(), "","");
}

// projects
string ifdh::startProject( string name, string station,  string defname_or_id,  string user,  string group) {

  if (name == "" && getenv("SAM_PROJECT"))
      name = getenv("SAM_PROJECT}");

  if (station == "" && getenv("SAM_STATION"))
      station = getenv("SAM_STATION}");

  return do_url_str(1,ssl_uri(_baseuri).c_str(),"startProject","","name",name.c_str(),"station",station.c_str(),"defname",defname_or_id.c_str(),"username",user.c_str(),"group",group.c_str(),"","");
}

string 
ifdh::findProject( string name, string station){
   
  if (name == "" && getenv("SAM_PROJECT"))
      name = getenv("SAM_PROJECT}");

  if (station == "" && getenv("SAM_STATIOn"))
      station = getenv("SAM_STATION}");

  return do_url_str(0,ssl_uri(_baseuri).c_str(),"findProject","","name",name.c_str(),"station",station.c_str(),"","");
}

string 
ifdh::establishProcess( string projecturi, string appname, string appversion, string location, string user, string appfamily , string description , int filelimit, string schemas ) {
  char buf[64];

  if (projecturi == "" && getenv("SAM_PROJECT") && getenv("SAM_STATION") ) {
      projecturi = this->findProject("","");
  }

  if (description == "" && getenv("JOBSUBJOBID")) {
      description = description + getenv("JOBSUBJOBID");
  }
  if (description == "" && getenv("CLUSTER") && getenv("PROCESS")) {
      description = description + getenv("CLUSTER") + "." + getenv("PROCESS");
  }

  if (filelimit == -1) {
     filelimit = 0;
  }

  snprintf(buf, 64, "%d", filelimit); 

  return do_url_str(1,projecturi.c_str(),"establishProcess", "", "appname", appname.c_str(), "appversion", appversion.c_str(), "deliverylocation", location.c_str(), "username", user.c_str(), "appfamily", appfamily.c_str(), "description", description.c_str(), "filelimit", buf, "schemas", schemas.c_str(), "", "");
}

string ifdh::getNextFile(string projecturi, string processid){
  if (projecturi == "" && getenv("SAM_PROJECT") && getenv("SAM_STATION") ) {
      projecturi = this->findProject("","");
  }
  return do_url_str(1,projecturi.c_str(),"processes",processid.c_str(),"getNextFile","","","");
}

string ifdh::updateFileStatus(string projecturi, string processid, string filename, string status){
  if (projecturi == "" && getenv("SAM_PROJECT") && getenv("SAM_STATION") ) {
      projecturi = this->findProject("","");
  }
  return do_url_str(1,projecturi.c_str(),"processes",processid.c_str(),"updateFileStatus","","filename", filename.c_str(),"status", status.c_str(), "","");
}

int 
ifdh::endProcess(string projecturi, string processid) {
  if (projecturi == "" && getenv("SAM_PROJECT") && getenv("SAM_STATION") ) {
      projecturi = this->findProject("","");
  }
  cleanup();
  return do_url_int(1,projecturi.c_str(),"processes",processid.c_str(),"endProcess","","","");
}

string 
ifdh::dumpProject(string projecturi) {
  if (projecturi == "" && getenv("SAM_PROJECT") && getenv("SAM_STATION") ) {
      projecturi = this->findProject("","");
  }
  return do_url_str(0,projecturi.c_str(), "summary","", "format", "json","","");
}

int ifdh::setStatus(string projecturi, string processid, string status){
  if (projecturi == "" && getenv("SAM_PROJECT") && getenv("SAM_STATION") ) {
      projecturi = this->findProject("","");
  }
  return do_url_int(1,projecturi.c_str(),"processes",processid.c_str(),"setStatus","","status",status.c_str(),"","");
}

int 
ifdh::endProject(string projecturi) {
  if (projecturi == "" && getenv("SAM_PROJECT") && getenv("SAM_STATION") ) {
      projecturi = this->findProject("","");
  }
  return do_url_int(1,projecturi.c_str(),"endProject","","","");
}

string 
ifdh::projectStatus(string projecturi) {
  if (projecturi == "" && getenv("SAM_PROJECT") && getenv("SAM_STATION") ) {
      projecturi = this->findProject("","");
  }
  return do_url_str(0,projecturi.c_str(),"status","","","");
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
    }
}


void
ifdh::set_base_uri(std::string baseuri) { 
    _baseuri = baseuri; 
}

#include <ifaddrs.h>
std::string
ifdh::unique_string() {
    uuid_t uuid;
    char uuidbuf[80];
    uuid_generate(uuid);
    uuid_unparse(uuid,uuidbuf);
    return std::string(uuidbuf);
}

// give output files reported with addOutputFile a unique name
int 
ifdh::renameOutput(std::string how) {
    std::string outfiles_name = datadir()+"/output_files";
    std::string new_outfiles_name = datadir()+"/output_files.new";
    std::string line;
    std::string file, infile, froms, tos, outfile;
    size_t spos;
    ifdh_op_msg mbuf("renameOutput", *this);

    if (how[0] == 's') {

        stringstream perlcmd;

        perlcmd << "perl -pi.bak -e '" 
                << "print STDERR  \"got line $_\";"
                << "@pair=split();"
                << "next unless $pair[0] && $pair[1];"
                << "$dir = $pair[0];"
                << "$pair[0] =~ s{.*/}{};"
                << "$dir =~ s{/?$pair[0]\\Z}{};"
                << "print STDERR  \"got dir $dir\";"
                << "$pair[1] =~ s{.*/}{};"
                << "$pair[1] =~ " << how << ";"
                << "rename(\"$dir/$pair[0]\", \"$dir/$pair[1]\");"
                << "print STDERR \"renaming $dir/$pair[0] to $dir/$pair[1]\\n\";"
                << "s{.*}{$dir/$pair[1] $dir/$pair[1]};"
                << "print STDERR \"result line: $_\";"
                << "' " << outfiles_name;
        

        _debug && std::cerr << "running: " << perlcmd.str() << "\n";

        cpn_lock locker;
        int res = retry_system(perlcmd.str().c_str(), 0, locker,mbuf,1);
        mbuf.log_success();
        return res;
    } else if (how[0] == 'e') {

        size_t loc = how.find(':');
        std::string cmd = how.substr(loc+1) + " " + outfiles_name;

        _debug && std::cerr << "running: " << cmd << "\n";

        cpn_lock locker;
        return retry_system(cmd.c_str(), 0, locker,mbuf,1);

    } else if (how[0] == 'u') {

        fstream outlog(outfiles_name.c_str(), ios_base::in);
        fstream newoutlog(new_outfiles_name.c_str(), ios_base::out);

        if ( outlog.fail()) {
          return 0;
        }

        while (!outlog.eof() && !outlog.fail()) {

            getline(outlog,line);
            spos = line.find(' ');
         
            file = line.substr(0,spos);
            infile = line.substr(spos+1, line.size() - spos - 1);

            if ( ! file.size() ) {
               break;
            }
  

            outfile = file;
            string uniq = unique_string();
            // don't expand repeatedly if renameOutput is called repeatedly
            // now using same rule as fife_utils  add_dataset
            // replace uuid's in the name.
            regexp uuid_re("-?[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}-?");
            regmatch m(uuid_re(file));
            
            if (m) {
                // trim out existing uuid before adding new one
                outfile = outfile.substr(0,m.data()[0].rm_so) + outfile.substr(m.data()[0].rm_eo);
            }

            spos = outfile.rfind('.');
            if (spos == string::npos) {
               spos = outfile.size();
            }
            
            outfile =  outfile.substr(0,spos) + "-" + uniq + outfile.substr(spos);
            rename(file.c_str(), outfile.c_str());

            newoutlog << outfile << " " << infile << "\n";
        }
        outlog.close();
        newoutlog.close();
        return rename(new_outfiles_name.c_str(), outfiles_name.c_str());
    }
    return -1;
}

// mostly a little named pipe plumbing and a fork()...
int
ifdh::more(string loc) {
    std::string where = localPath(loc);
    const char *c_where = where.c_str();
    int res2 ,res;

    // need tos set DATA_DIR so grandkids agree on where it is..
    char denvbuf[MAXPATHLEN];
    strcpy(denvbuf,"IFDH_DATA_DIR=");
    strcpy(denvbuf+14, localPath("").c_str());
    putenv(denvbuf);

    res  = mknod(c_where, 0600 | S_IFIFO, 0);
    if (res == 0) {
       res2 = fork();
       if (res2 == 0) {
            execlp( "more", "more", c_where,  NULL);
       } else if (res2 > 0) {
            // turn off retries
            char envbuf[] = "IFDH_CP_MAXRETRIES=0\0\0\0\0";
            const char *was = getenv("IFDH_CP_MAXRETRIES");
            putenv(envbuf);

            // now fetch the file to the named pipe
            try {
                fetchInput(loc);
            } catch (...) {
		// unwedge other side if cp failed
		int f = open(c_where,O_WRONLY|O_NDELAY, 0700);
		if (f >= 0) {
		    close(f);
		}
                res = 1;
            }
            waitpid(res2, 0,0);
            unlink(c_where);

            // put back retries
            if (!was)
               was = "0";
            strcpy(envbuf+17,was);
            putenv(envbuf);
       } else {
            return -1;
       }
    }
    return res;
}

int
ifdh::apply(std::vector<std::string> args) {
    std::vector<std::pair<std::string,long> > rlist;

    std::string dir = args[0];
    std::string pat = args[1];
    args.erase(args.begin(), args.begin()+2);
    std::string subdir;
    int res = 0;
    int rres = 0;

    if (dir[dir.size()-1] != '/') {
        dir = dir + "/";
    }

    rlist = findMatchingFiles(dir, pat);

    std::string tmplcmd = "ifdh " + join(args, ' ');

    for( size_t i = 0; i < rlist.size(); i++ ) {
        std::string file = rlist[i].first;
	std::string cmd;
        std::cout << file << ":\n";
        std::cout.flush();

        if (has(file, "/")) {
            subdir = file.substr(dir.size(),file.rfind('/'));
        } else {
            subdir = "";
        }

	cmd = tmplcmd;

        if ( has(cmd,"%(file)s") ) {
            cmd = cmd.replace(cmd.find("%(file)s"), 8, file);
        }
        if ( has(cmd,"%(subdir)s") ) {
           cmd = cmd.replace(cmd.find("%(subdir)s"), 10, subdir);
        }
        _debug && std::cerr << "running: " << cmd << "\n";

        res = system(cmd.c_str());
        if (res != 0) {
            rres = res;
        } 
    }
    return rres;
}

// mostly a little named pipe plumbing and a fork()...
std::string
ifdh::checksum(string loc) {
    if(_debug) cerr << "starting cheksum( " << loc << ")" << endl;
    cerr.flush();
    std::stringstream sumtext;
    std::string where = localPath(loc);
    std::string path;
    const char *c_where = where.c_str();
    int res2 ,res;
    unsigned long sum0, sum1;
    res  = mknod(c_where, 0600 | S_IFIFO, 0);
    if(_debug) cerr << "made node " << c_where << endl;
 
    // need tos set DATA_DIR so grandkids agree on where it is..
    char denvbuf[MAXPATHLEN];
    strcpy(denvbuf,"IFDH_DATA_DIR=");
    strcpy(denvbuf+14, localPath("").c_str());
    putenv(denvbuf);

    if (res == 0) {
       int status;
       unsigned char md5digest[16];
       res2 = fork();
       if(_debug) cerr << "fork says " << res2 << endl;
       if (res2 > 0) {

            if(_debug) cerr << "starting get_adler32( " << c_where << ")" << endl;
            checksum::get_sums(c_where, &sum0, &sum1, md5digest);

            if(_debug) cerr << "finished get_adler32( " << c_where << ")" << endl;
            waitpid(res2, &status,0);
            unlink(c_where);
            if ( WIFEXITED(status) && WEXITSTATUS(status)==0) {
                // only fill in sum text if the fetchinput side succeeded...
	        sumtext <<  "[ "
                    << "\"enstore:" << std::dec << sum0 << "\","
                    << "\"adler32:" << std::hex << setw(8) << setfill('0') << sum1 << "\","
                    << "\"md5:"; 
                for (int i = 0 ; i< 16; i++ ) {
                    sumtext << std::hex << setw(2) << setfill('0');
                    sumtext << (unsigned int)(md5digest[i]);
                }
	        sumtext << "\"" << " ]" << endl << std::dec << setfill(' ') << setw(0);
            }
       } else if (res2 == 0) {

            // turn off retries
            char envbuf[] = "IFDH_CP_MAXRETRIES=0\0\0\0\0";
            putenv(envbuf);

            res = 0;
            if(_debug) cerr << "starting fetchInput( " << loc << ")" << endl;
            try {
                path = fetchInput(loc);
            } catch (...) {
		// unwedge other side if cp failed
		int f = open(c_where,O_WRONLY|O_NDELAY, 0700);
		if (f >= 0) {
		    close(f);
		}
                res = 1;
            }
            if(_debug) cerr << "finished fetchInput( " << loc << ")" << endl;
            exit(res);

       } else {
            throw( std::logic_error("fork failed"));
           
       }
    } else {
       throw( std::logic_error("mknod failed"));
    }
    return sumtext.str();
}

std::map<std::string,std::vector<std::string> > 
ifdh::locateFiles( std::vector<std::string> args) {
    stringstream url;
    stringstream postdata;
    std::map<std::string,std::vector<std::string> >  res;
    std::vector<std::string> locvec;
    size_t p1, p2, p3, start;
    std::string line, fname;

    if (_debug) cerr << "Entering LocateFiles -- _baseuri is " << _baseuri; 

    url << _baseuri.c_str() << "/files/locations";
    for(size_t i = 0; i < args.size(); ++i) {
        postdata << "file_name=" << args[i] << "&";
    }
    postdata << "format=json";
    WebAPI wa(url.str(), 1, postdata.str());
    
    while (!wa.data().eof() && !wa.data().fail()) {
        getline(wa.data(), line, ']');
        p1 = line.find('"');
        p2 = line.find('"', p1+1);
        if (p1 != string::npos && p2 != string::npos) {
            locvec.clear();
            fname = line.substr(p1+1,p2-p1-1);
            start = p2+2;
            p3 = line.find("\"location\":", start);
            while (p3 != string::npos) {
                p1 = line.find('"', p3+10);
                p2 = line.find('"', p1+1);
                if (p1 != string::npos && p2 != string::npos) {
                   locvec.push_back(line.substr(p1+1, p2-p1-1));
                }
                start = p2+1;
                p3 = line.find("\"location\":", start);
           }
           res[fname] = locvec;
       }
    }
    return res;
}


}

