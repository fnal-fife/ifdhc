#include "ifdh.h"
#include "utils.h"
#include <fcntl.h>
#include <string>
#include <sstream>
#include <iostream>
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
#include <setjmp.h>
#include <memory>
#include <gnu/libc-version.h>

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
    string curpath(getenv("PATH"));

    setenvbuf << s1 << s2 << ":" << curpath;
    setenv("PATH", setenvbuf.str().c_str(), 1);
}

void
path_ish_append(const char *what, string s1, string s2) {
    stringstream setenvbuf;
    string curpath(getenv(what));

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
              
        string path(getenv("PATH"));
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

    // put cmvfs OSG utils at the end of our path as a failover/fallback
    // currently assuming 64bit is okay
    int glibcminor = atoi(gnu_get_libc_version()+2);
    int slver = glibcminor > 5 ? (glibcminor > 12 ? 7 : 6) : 5;
    stringstream cvmfs_dir;
    cvmfs_dir << "/cvmfs/oasis.opensciencegrid.org/mis/osg-wn-client/3.3/current/el" <<  slver << "-x86_64";
    path_ish_append("PATH",cvmfs_dir.str().c_str(),"/usr/bin");
    path_ish_append("LD_LIBRARY_PATH",cvmfs_dir.str().c_str(),"/usr/lib64");
}

string cpn_loc  = "cpn";  // just use the one in the PATH -- its a product now
string fermi_gsiftp  = "gsiftp://fg-bestman1.fnal.gov:2811";
string bestmanuri = "srm://fg-bestman1.fnal.gov:10443/srm/v2/server?SFN=";
std::string ifdh::_default_base_uri = "http://samweb.fnal.gov:8480/sam/";
std::string ifdh::_default_base_ssl_uri = "https://samweb.fnal.gov:8483/sam/";

string ssl_uri(string s) {
   if (s.find(ifdh::_default_base_uri) == 0) {
      return ifdh::_default_base_ssl_uri + s.substr(ifdh::_default_base_uri.length());
   } else {
      if (getenv("IFDH_BASE_URI") && getenv("IFDH_BASE_SSL_URI") && s.find(getenv("IFDH_BASE_URI")) == 0)
          return getenv("IFDH_BASE_SSL_URI") + s.substr(strlen(getenv("IFDH_BASE_URI")));
      return s;
   }
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
        if (icx && atoi(icx)) {
             path = localPath( src_uri );
             string cmd("xrdcp ");
             cmd += src_uri + " ";
             cmd += path + " ";
             _debug && std::cerr << "running: " << cmd << std::endl;
             int status = system(cmd.c_str());
             if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
                 return path;
             else
                 return "";
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
    try {
       if ( 0 == cp( args ) && 0 == access(path.c_str(),R_OK)) {
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

int 
ifdh::copyBackOutput(string dest_dir) {
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
        cpargs.push_back(dest_dir + "/" + filelast);
        _debug && std::cerr << "adding cp of " << file << " " << dest_dir << "/" << filelast << "\n";
   
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


void
handle_timeout(int what) {
    what = what;
    std::cerr << "Timeout in ifdh web call\n";
    throw std::runtime_error("Timeout in ifdh web call");
}


// make a timeout class so we never forget to 
// clean up our siglarm, even if we catch an exception
//
class timeoutobj {
    struct sigaction oldaction;
    int oldalarm;

    void
    set_timeout() {
	int timeoutafter;
	struct sigaction action;
	memset(&action, 0, sizeof(struct sigaction));
	sigset_t empty;
	sigemptyset(&empty);
	action.sa_handler = handle_timeout;
	action.sa_mask = empty;
       
	if (getenv("IFDH_WEB_TIMEOUT")) { 
	    timeoutafter = atoi(getenv("IFDH_WEB_TIMEOUT"));
	} else { 
	    timeoutafter = 3*60*60;
	}
        ifdh::_debug && std::cerr << "set_timeout: setting alarm to " << timeoutafter << "\n";
	sigaction(SIGALRM, &action, &oldaction);
	oldalarm = alarm(timeoutafter);
    }

    void
    clear_timeout() {
        ifdh::_debug && std::cerr << "set_timeout: setting alarm to " << oldalarm  << "\n";
	alarm(oldalarm);
	sigaction(SIGALRM, &oldaction, (struct sigaction*)0);
    }
public:
    timeoutobj() { set_timeout(); };
    ~timeoutobj() { clear_timeout(); };
};

int
do_url_int(int postflag, ...) {
    va_list ap;
    int res;

    va_start(ap, postflag);
    try {
       class timeoutobj to;
       unique_ptr<WebAPI> wap(do_url_2(postflag, ap));
       res = wap->getStatus() - 200;
    } catch( exception &e )  {
       std::cerr << "Exception: " << e.what();
       res = 300;
    }
    if (ifdh::_debug) std::cerr << "got back int result: " << res << "\n";
    return res;
}


string
do_url_str(int postflag,...) {
    va_list ap;
    string res("");
    string line;
    try {
        class timeoutobj to;
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
       std::cerr << "Exception: " << e.what();
       return "";
    }
    if (ifdh::_debug) std::cerr << "got back string result: " << res << "\n";
    return res;
}

vector<string>
do_url_lst(int postflag,...) {
    va_list ap;
    string line;
    vector<string> empty;
    vector<string> res;
    try {
        class timeoutobj to;
	va_start(ap, postflag);
	unique_ptr<WebAPI> wap(do_url_2(postflag, ap));
	while (!wap->data().eof() && !wap->data().fail()) {
	    getline(wap->data(), line);
	    if (! (line == "" && wap->data().eof())) {
		res.push_back(line);
	    }
	}
    } catch( exception &e )  {
       std::cerr << "Exception: " << e.what();
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

//datasets
int 
ifdh::createDefinition( string name, string dims, string user, string group) {
  return do_url_int(1,ssl_uri(_baseuri).c_str(),"createDefinition","","name",name.c_str(), "dims", dims.c_str(), "user", user.c_str(),"group", group.c_str(), "","");
}

int 
ifdh::deleteDefinition( string name) {
  return  do_url_int(1,_baseuri.c_str(),"definitions","name", name.c_str(),"delete","","");
}

string 
ifdh::describeDefinition( string name) {
  return do_url_str(0,_baseuri.c_str(),"definitions", "name", name.c_str(), "describe",  "","");
}

vector<string> 
ifdh::translateConstraints( string dims) {
  return do_url_lst(0,_baseuri.c_str(),"files", "list", "", "dims", dims.c_str(), "","" );
}

// files
vector<string> 
ifdh::locateFile( string name, string schema ) {
  if (schema == "" ) {
      return do_url_lst(0,_baseuri.c_str(), "files", "name", name.c_str(), "locations", "", "" );  
  } else {
      return do_url_lst(0,_baseuri.c_str(), "files", "name", name.c_str(), "locations", "url", "" , "schema", schema.c_str(), "", "");
  }
}

string ifdh::getMetadata( string name) {
  return  do_url_str(0, _baseuri.c_str(),"files","name", name.c_str(), "metadata","","");
}

//
string 
ifdh::dumpStation( string name, string what ) {
  
  if (name == "" && getenv("SAM_STATION"))
      name = getenv("SAM_STATION}");

  return do_url_str(0,_baseuri.c_str(),"dumpStation", "", "station", name.c_str(), "dump", what.c_str(), "","");
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

int ifdh::endProject(string projecturi) {
  if (projecturi == "" && getenv("SAM_PROJECT") && getenv("SAM_STATION") ) {
      projecturi = this->findProject("","");
  }
  return do_url_int(1,projecturi.c_str(),"endProject","","","");
}

extern int host_matches(std::string glob);

ifdh::ifdh(std::string baseuri) {
    check_env();
    char *debug = getenv("IFDH_DEBUG");
    if (0 != debug ) { 
        std::cerr << "IFDH_DEBUG=" << debug << " => " << atoi(debug) << "\n";
	_debug = atoi(debug);
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
    const char *ccffile = getenv("IFDHC_CONFIG_DIR");
    if (ccffile == 0) {
        ccffile = getenv("IFDHC_DIR");
        _debug && std::cerr << "ifdh: getting config file from IFDHC_DIR --  no IFDHC_CONFIG_DIR?!?\n";
    }
    if (ccffile == 0) {
	throw( std::logic_error("no ifdhc config file environment variables found"));
    }
    _debug && std::cerr << "ifdh: using config file: "<< ccffile << "/ifdh.cfg\n";
    std::string cffile(ccffile);

    _config.read(cffile + "/ifdh.cfg");
    std::vector<std::string> clist = _config.getlist("general","conditionals");
    _debug && std::cerr << "checking conditionals:\n";
    for( size_t i = 0; i < clist.size(); i++ ) { 
	_debug && std::cerr << "conditional" << i << ": " << clist[i] << "\n";
        if (clist[i] == "") {
           continue;
        }
        std::string rtype;
        std::string tststr = _config.get("conditional " + clist[i], "test");
        std::vector<std::string> renamevec = _config.getlist("conditional " + clist[i], "rename_proto");
        if (renamevec.size() > 0) {
           rtype = "protocol ";
        } else {
           renamevec = _config.getlist("conditional " + clist[i], "rename_loc");
           if (renamevec.size() > 0) {
               rtype = "location ";
           }
        }
        if (tststr[0] == '-' && tststr[1] == 'x') {
            if (0 == access(tststr.substr(3).c_str(), X_OK)) {
                _debug && std::cerr << "test: " << tststr << " renaming: " << renamevec[0] << " <= " << renamevec[1] << "\n";
		_config.rename_section(rtype + renamevec[0], rtype + renamevec[1]);
            }
            continue;
        }
        if (tststr[0] == '-' && tststr[1] == 'r') {
            if (0 == access(tststr.substr(3).c_str(), R_OK)) {
                _debug && std::cerr << "test: " << tststr << " renaming: " << renamevec[0] << " <= " << renamevec[1] << "\n";
		_config.rename_section(rtype + renamevec[0], rtype + renamevec[1]);
            }
            continue;
        }
        if (tststr[0] == '-' && tststr[1] == 'H') {
           if (host_matches(tststr.substr(3))) {
                _debug && std::cerr << "test: " << tststr << " renaming: " << renamevec[0] << " <= " << renamevec[1] << "\n";
		_config.rename_section(rtype + renamevec[0], rtype + renamevec[1]);
           }
           continue;
        }
    }
    // handle protocol aliases
    std::vector<std::string> plist = _config.getlist("general","protocols");
    std::string av;
    for( size_t i = 0; i < plist.size(); i++ ) { 
        av =  _config.get("protocol " + plist[i], "alias");
        if (av != "" ) {
             _config.copy_section("protocol " + av, "protocol " + plist[i]);
        }
    }
    // -------------------------------------------------------
    }
}

void
ifdh::set_base_uri(std::string baseuri) { 
    _baseuri = baseuri; 
}

std::string
ifdh::unique_string() {
    static int count = 0;
    char hbuf[512];
    gethostname(hbuf, 512);
    time_t t = time(0);
    int pid = getpid();
    stringstream uniqstr;

    uniqstr << '_' << hbuf << '_' << t << '_' << pid << '_' << count++;
    return std::string(uniqstr.str());
}

// give output files reported with addOutputFile a unique name
int 
ifdh::renameOutput(std::string how) {
    std::string outfiles_name = datadir()+"/output_files";
    std::string new_outfiles_name = datadir()+"/output_files.new";
    std::string line;
    std::string file, infile, froms, tos, outfile;
    size_t spos;
   


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

        return system(perlcmd.str().c_str());
    
    } else if (how[0] == 'e') {

        size_t loc = how.find(':');
        std::string cmd = how.substr(loc+1) + " " + outfiles_name;

        _debug && std::cerr << "running: " << cmd << "\n";

        return system(cmd.c_str());

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
  
            spos = file.find('.');
            if (spos == string::npos) {
               spos = file.length();
            }


            outfile = file;
            string uniq = unique_string();
            // don't double-uniqify if renameOutput is called repeatedly
            //  -- i.e. if the uniqe string without the pid and counter 
            //      on the end already exists in the filename
            if (string::npos == outfile.find(uniq.substr(0,uniq.size()-8))) {
                outfile = outfile.insert( spos, uniq );
	        rename(file.c_str(), outfile.c_str());
	        _debug && std::cerr << "renaming: " << file << " " << outfile << "\n";
            }

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
            std::cerr << "dir " << dir << " subdir " << subdir << "\n";
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
    unsigned long sum;
    res  = mknod(c_where, 0600 | S_IFIFO, 0);
    if(_debug) cerr << "made node " << c_where << endl;

    if (res == 0) {
       int status;
       res2 = fork();
       if(_debug) cerr << "fork says " << res2 << endl;
       if (res2 > 0) {
            // turn off retries
            char envbuf[] = "IFDH_CP_MAXRETRIES=0\0\0\0\0";
            const char *was = getenv("IFDH_CP_MAXRETRIES");
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

            // put back retries
            if (!was)
               was = "0";
            strcpy(envbuf+17,was);
            putenv(envbuf);

            waitpid(res2, &status,0);
            unlink(c_where);

            exit(res == 0 ? status : res);
       } else if (res2 == 0) {
            if(_debug) cerr << "starting get_adler32( " << c_where << ")" << endl;
            sum = checksum::get_adler32(c_where);
	    sumtext <<  "{\"crc_value\": \""  
                    << sum
                    << "\", \"crc_type\": \"adler 32 crc type\"}"
                    << endl;
            if(_debug) cerr << "finished get_adler32( " << c_where << ")" << endl;
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
