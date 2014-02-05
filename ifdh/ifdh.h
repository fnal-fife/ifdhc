
#ifndef IFDH_H
#define IFDH_H
#include <string>
#include <vector>
#include "../util/WebAPI.h"
#include <stdlib.h>


namespace ifdh_ns {

class ifdh {
       
        std::string _baseuri;
        std::string _lastinput;
        std::string unique_string();
        std::vector<std::string> build_stage_list( std::vector<std::string>, int, char *stage_via);
   public:
        static int _debug;
        static std::string _default_base_uri;

        // generic constructor...

        ifdh(std::string baseuri = "");
        void set_debug(std::string s) { _debug = s[0] - '0';}

        void set_base_uri(std::string baseuri);

        // general file copy using cpn locks dd, gridftp, or srmcp
        // supports:
        // * cp src1 dest1 [';' src2 dest2 [';'...]]
        // * cp -r src1 dest1 [';' src2 dest2 [';'...]]
        // * cp -D src1 src2 destdir1 [';' src3 src4 destdir2 [';'...]]
        // * cp -f file_with_src_space_dest_lines
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

	// locate a file
	std::vector<std::string> locateFile( std::string name);
        // get a files metadata
	std::string getMetadata( std::string name);

	// give a dump of a SAM station status
	std::string dumpStation( std::string name, std::string what = "all");

	// start a new file delivery project
	std::string startProject( std::string name, std::string station,  std::string defname_or_id,  std::string user,  std::string group);
        // find a started project
	std::string findProject( std::string name, std::string station);

        // set yourself up as a file consumer process for a project
	std::string establishProcess(std::string projecturi,  std::string appname, std::string appversion, std::string location, std::string user, std::string appfamily = "", std::string description = "", int filelimit = -1);
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
        std::vector<std::string> ls( std::string loc, int recursion_depth, std::string force);
        int mkdir(std::string loc, std::string force);
        int rm(std::string loc, std::string force);
        int rmdir(std::string loc, std::string force);
};

}

using namespace ifdh_ns;
#endif // IFDH_H
