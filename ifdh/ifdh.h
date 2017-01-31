
#ifndef IFDH_H
#define IFDH_H
#include <string>
#include <vector>
#include "../util/WebAPI.h"
#include "../util/WimpyConfigParser.h"
#include <stdlib.h>
#include <map>
#include <utility>


namespace ifdh_ns {

class  cpn_lock;
struct IFile;
struct CpPair;

class ifdh {
        std::string _baseuri;
        std::string _lastinput;
        std::string unique_string();
        std::vector<std::string> build_stage_list( std::vector<std::string>, int, char *stage_via);
   public:
        static WimpyConfigParser _config;
        static int _debug;
        static std::string _default_base_uri;
        static std::string _default_base_ssl_uri;

        // generic constructor...

        ifdh(std::string baseuri = "");
        void set_debug(std::string s) { _debug = s[0] - '0';}

        void set_base_uri(std::string baseuri);

        // general file copy using cpn locks dd, gridftp, or srmcp
        // supports:
        //
        // * cp src1 dest1 [';' src2 dest2 [';'...]]                     -- basic source/dest filenames
        // * cp -r src1 dest1 [';' src2 dest2 [';'...]]                  -- recursive directory copies
        // * cp -D src1 src2 destdir1 [';' src3 src4 destdir2 [';'...]]  -- copies to dest. directory
        // * cp -f file_with_src_space_dest_lines                        -- copies to a list file
        // * any of the above can take --force={cpn,gridftp,srmcp,expgridftp}
        // * any of the file/dest arguments can be URIs
        // ---
        int cp(std::vector<std::string> args);

	// get input file to local scratch, return scratch location
	std::string fetchInput( std::string src_uri );

	// return scratch location fetchInput would give, without copying
	std::string localPath( std::string src_uri );

	// add output file to set
	int addOutputFile(std::string filename);

	// copy output file set to destination with cpn or srmcp
	int copyBackOutput(std::string dest_dir);

	// logging
	int log( std::string message );

        // log entering/leaving states
	int enterState( std::string state );
	int leaveState( std::string state );

	// make a named dataset definition from a dimension string
	int createDefinition( std::string name, std::string dims, std::string user, std::string group);
	// remove data set definition
	int deleteDefinition( std::string name);
        // describe a named dataset definition
	std::string describeDefinition( std::string name);
        // give file list for dimension string
	std::vector<std::string> translateConstraints( std::string dims);

	// locate a file, with optional schema (gsiftp, etc.) 
	std::vector<std::string> locateFile( std::string name, std::string schema = "");
        // get a files metadata
	std::string getMetadata( std::string name);

	// give a dump of a SAM station status
	std::string dumpStation( std::string name, std::string what = "all");

	// start a new file delivery project
	std::string startProject( std::string name, std::string station,  std::string defname_or_id,  std::string user,  std::string group);
        // find a started project
	std::string findProject( std::string name, std::string station);

        // set yourself up as a file consumer process for a project
	std::string establishProcess(std::string projecturi,  std::string appname, std::string appversion, std::string location, std::string user, std::string appfamily = "", std::string description = "", int filelimit = -1, std::string schemas = "");
        // get the next file location from a project
	std::string getNextFile(std::string projecturi, std::string processid);
        // update the file status (use: transferred, skipped, or consumed)
	std::string updateFileStatus(std::string projecturi, std::string processid, std::string filename, std::string status);
        // end the process
        int endProcess(std::string projecturi, std::string processid);
        // say what the sam station knows about your process
        std::string dumpProject(std::string projecturi);
        // set process status
	int setStatus(std::string projecturi, std::string processid, std::string status);
        // end the project
        int endProject(std::string projecturi);
        // clean up any tmp file stuff
        int cleanup();
        // give output files reported with addOutputFile a unique name
        int renameOutput(std::string how);
        // general file rename using mvn or srmcp
        int mv(std::vector<std::string> args);
        // Get a list of directory contents, or check existence of files
        // use recursion_depth== 0 to check directory without contents
        std::vector<std::string> ls( std::string loc, int recursion_depth, std::string force = "");
        // make a directory (i.e. for file destination)
        int mkdir(std::string loc, std::string force = "");
        // remove files
        int rm(std::string loc, std::string force = "");
        // remove directories
        int rmdir(std::string loc, std::string force = "");
        // view text files
        int more(std::string loc);
        // pin file 
        int pin(std::string loc, long int seconds);
        // change file permissions
        int chmod(std::string mode, std::string loc, std::string force = "");
        // atomic rename items in same directory/fs
        int rename(std::string loc, std::string loc2,  std::string force = "");
        // list files with long listing
        // use recursion_depth== 0 to check directory without contents
        int ll( std::string loc, int recursion_depth, std::string force = "");
        // list files with sizes
        // use recursion_depth== 0 to check directory without contents
        std::vector<std::pair<std::string,long> > lss( std::string loc, int recursion_depth, std::string force = "");
        // find filenames and sizes matching pattern
        std::vector<std::pair<std::string,long> > findMatchingFiles( std::string path, std::string glob); 
        // filenames and sizes matching pattern moved locally enough to be seen
        std::vector<std::pair<std::string,long> > fetchSharedFiles( std::vector<std::pair<std::string,long> > list, std::string schema = ""); 
        // locate multiple files
        std::map<std::string,std::vector<std::string> > locateFiles( std::vector<std::string> args );
        // cheksum file
        std::string checksum(std::string loc);
        // make a directory with intervening directories
        int mkdir_p(std::string loc, std::string force = "", int depth = -1);
        // get a grid proxy for the current experiment if needed, 
        // return the path
        std::string getProxy();
        // declare file metadata
	int declareFile( std::string json_metadata);
        // modify file metadata
	int modifyMetadata(std::string file,  std::string json_metadata);
        // apply an ifdh command to all files under a directory 
        // (recursively), matching a pattern
        int apply(std::vector<std::string> args);
    private:
        IFile lookup_loc(std::string url) ;
        std::string locpath(IFile loc, std::string proto) ;
        int retry_system(const char *cmd_str, int error_expected, cpn_lock &locker,   int maxtries = -1, std::string unlink_on_error = "") ;
        std::string srcpath(CpPair &cpp) ;
        std::string dstpath(CpPair &cpp) ;
        int do_cp_bg(CpPair &cpp, bool intermed_file_flag, bool recursive, cpn_lock &cpn);
        int do_cp(CpPair &cpp,  bool intermed_file_flag, bool recursive, cpn_lock &cpn) ;
	void pick_proto(CpPair &p, std::string force) ;
        std::vector<CpPair> handle_args( std::vector<std::string> args, std::vector<std::string>::size_type curarg, bool dest_is_dir,  size_t &lock_low, size_t &lock_hi, std::string &force);
        bool have_stage_subdirs(std::string uri);
        void pick_proto_path(std::string loc, std::string force, std::string &proto, std::string &fullurl, std::string &lookup_proto );
};
}

using namespace ifdh_ns;
#endif // IFDH_H
