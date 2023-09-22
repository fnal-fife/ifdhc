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
       if ( _config.get("general","version") != "") {
           _config_version = _config.get("general","version");
       }
       _debug && std::cerr << "ifdh: config file version is " << _config_version << "\n";
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

