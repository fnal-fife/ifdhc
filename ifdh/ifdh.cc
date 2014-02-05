#include "ifdh.h"
#include "utils.h"
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

using namespace std;

namespace ifdh_ns {

int
ifdh::_debug = 0;

void
path_prepend( string s1, string s2) {
    stringstream setenvbuf;
    string curpath(getenv("PATH"));

    setenvbuf << s1 << s2 << ":" << curpath;
    setenv("PATH", setenvbuf.str().c_str(), 1);
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
}

string cpn_loc  = "cpn";  // just use the one in the PATH -- its a product now
string fermi_gsiftp  = "gsiftp://fg-bestman1.fnal.gov:2811";
string bestmanuri = "srm://fg-bestman1.fnal.gov:10443/srm/v2/server?SFN=";
std::string ifdh::_default_base_uri = "http://samweb.fnal.gov:8480/sam/";

string datadir() {
    stringstream dirmaker;
    string localpath;
    int res;
    
    dirmaker << (
       getenv("_CONDOR_SCRATCH_DIR")?getenv("_CONDOR_SCRATCH_DIR"):
       getenv("TMPDIR")?getenv("TMPDIR"):
       "/var/tmp"
    )
       << "/ifdh_" << getppid();

    if ( 0 != access(dirmaker.str().c_str(), W_OK) ) {
        res = mkdir(dirmaker.str().c_str(),0700);
        ifdh::_debug && cout <<  "mkdir " << dirmaker.str() << " => " << res << "\n";
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

extern "C" { const char *get_current_dir_name(); }

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

    if (0 == src_uri.find("xrootd:")) {
        // we don't do anything for xrootd, just pass
        // it back, and let the application layer open
        // it that way.
        return src_uri;
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

        _debug && std::cout << "parsing: " << line << "\n";
	spos = line.find(' ');
	if (spos != string::npos) {
	    file = line.substr(0,spos);
	} else {
	    file = line;
	}
	if ( !file.size() ) {
	   break;
	}
	spos = line.rfind('/');
	if (spos != string::npos) {
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
   
    }
    return cp(cpargs);
}

// logging
int 
ifdh::log( string message ) {
  if (!numsg::getMsg()) {
      numsg::init(getexperiment(),1);
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
        (postflag ? postdata : url)  << sep << WebAPI::encode(name) << "=" << WebAPI::encode(val);
        sep = "&";
        name = va_arg(ap,char *);
        val = va_arg(ap,char *);
    }
    urls = url.str();
    postdatas= postdata.str();

    if (ifdh::_debug) std::cerr << "calling WebAPI with url: " << urls << " and postdata: " << postdatas << "\n";

    return new WebAPI(urls, postflag, postdatas);
}

int
do_url_int(int postflag, ...) {
    va_list ap;
    int res;
    va_start(ap, postflag);
    WebAPI *wap = do_url_2(postflag, ap);
    res = wap->getStatus() - 200;
    if (ifdh::_debug) std::cerr << "got back int result: " << res << "\n";
    delete wap;
    return res;
}


string
do_url_str(int postflag,...) {
    va_list ap;
    string res("");
    string line;
    va_start(ap, postflag);
    WebAPI *wap = do_url_2(postflag, ap);
    while (!wap->data().eof()) {
      getline(wap->data(), line);
      if (wap->data().eof()) {
         res = res + line;
      } else {
         res = res + line + "\n";
      }
    }
    if (ifdh::_debug) std::cerr << "got back string result: " << res << "\n";
    delete wap;
    return res;
}

vector<string>
do_url_lst(int postflag,...) {
    va_list ap;
    string line;
    vector<string> res;
    va_start(ap, postflag);
    WebAPI *wap = do_url_2(postflag, ap);
    while (!wap->data().eof()) {
        getline(wap->data(), line);
        res.push_back(line);
    }
    delete wap;
    return res;
}

//datasets
int 
ifdh::createDefinition( string name, string dims, string user, string group) {
  return do_url_int(1,_baseuri.c_str(),"createDefinition","","name",name.c_str(), "dims", dims.c_str(), "user", user.c_str(),"group", group.c_str(), "","");
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
ifdh::locateFile( string name) {
  return do_url_lst(0,_baseuri.c_str(), "files", "name", name.c_str(), "locations", "", "" );  
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

  if (station == "" && getenv("SAM_STATIOn"))
      station = getenv("SAM_STATION}");

  return do_url_str(1,_baseuri.c_str(),"startProject","","name",name.c_str(),"station",station.c_str(),"defname",defname_or_id.c_str(),"username",user.c_str(),"group",group.c_str(),"","");
}

string 
ifdh::findProject( string name, string station){
   
  if (name == "" && getenv("SAM_PROJECT"))
      name = getenv("SAM_PROJECT}");

  if (station == "" && getenv("SAM_STATIOn"))
      station = getenv("SAM_STATION}");

  return do_url_str(0,_baseuri.c_str(),"findProject","","name",name.c_str(),"station",station.c_str(),"","");
}

string 
ifdh::establishProcess( string projecturi, string appname, string appversion, string location, string user, string appfamily , string description , int filelimit ) {
  char buf[64];

  if (projecturi == "" && getenv("SAM_PROJECT") && getenv("SAM_STATION") ) {
      projecturi = this->findProject("","");
  }

  if (filelimit == -1) {
     filelimit = 0;
  }

  snprintf(buf, 64, "%d", filelimit); 

  return do_url_str(1,projecturi.c_str(),"establishProcess", "", "appname", appname.c_str(), "appversion", appversion.c_str(), "deliverylocation", location.c_str(), "username", user.c_str(), "appfamily", appfamily.c_str(), "description", description.c_str(), "filelimit", buf, "", "");
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

ifdh::ifdh(std::string baseuri) { 
    check_env();
    if (0 != getenv("IFDH_DEBUG")) { 
	_debug = 1;
    }
    if ( baseuri == "" ) {
        _baseuri = getenv("IFDH_BASE_URI")?getenv("IFDH_BASE_URI"):(getenv("EXPERIMENT")?_default_base_uri+getenv("EXPERIMENT")+"/api":"") ;
     } else {
       _baseuri = baseuri;
    }
    _debug && std::cerr << "ifdh constructor: _baseuri is '" << _baseuri << "'\n";
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
    return uniqstr.str();
}

// give output files reported with addOutputFile a unique name
int 
ifdh::renameOutput(std::string how) {
    std::string outfiles_name = datadir()+"/output_files";
    std::string new_outfiles_name = datadir()+"/output_files.new";
    std::string line;
    fstream outlog(outfiles_name.c_str(), ios_base::in);
    fstream newoutlog(new_outfiles_name.c_str(), ios_base::out);
    std::string file, infile, froms, tos, outfile;
    int res;
    size_t spos;
   

    if ( outlog.fail()) {
       return 0;
    }

    if (how[0] == 's') {

        spos = how.find('/',2);
        froms = how.substr( 2, spos - 2);
        tos = how.substr( spos+1, how.size() - spos - 2);

        _debug && std::cerr << "replacing " << froms << " with " << tos << "\n";

        while (!outlog.eof() && !outlog.fail()) {
            getline(outlog,line);
            spos = line.find(' ');
         
            file = line.substr(0,spos);
            infile = line.substr(spos+1, line.size() - spos - 1);

            if ( ! file.size() ) {
               break;
            }
            if ( ! infile.size() ) {
               std::cerr << "Notice: renameOutput found no input file name to base renaming on!\n";
            }
  
            spos = infile.rfind(froms);
            if (string::npos != spos) { 
                outfile = infile;
                outfile = outfile.replace(spos, froms.size(), tos);
                _debug && std::cerr << "renaming: " << file << " " << outfile << "\n";
                res = rename(file.c_str(), outfile.c_str());
                if ( 0 != res ) {
                    throw(WebAPIException("rename failed:", file.c_str() ));
                }
            } else {
                outfile = file;
            }
            newoutlog << outfile << " " << infile << "\n";

        }
    } else if (how[0] == 'u') {


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
            outfile = outfile.insert( spos, unique_string() );
	    rename(file.c_str(), outfile.c_str());

	    _debug && std::cerr << "renaming: " << file << " " << outfile << "\n";
            newoutlog << outfile << " " << infile << "\n";
        }
    }
    outlog.close();
    newoutlog.close();
    return rename(new_outfiles_name.c_str(), outfiles_name.c_str());
}

}
