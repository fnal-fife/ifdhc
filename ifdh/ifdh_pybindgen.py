
import pybindgen
import sys
from pybindgen import Module, retval, param

mod=pybindgen.Module("ifdh",cpp_namespace="ifdh_ns")
mod.add_include('"ifdh.h"')

pair = mod.add_struct('ifdh_lss_pair')
pair.add_instance_attribute('first','std::string')
pair.add_instance_attribute('second','long')

mod.add_container('std::vector<ifdh_lss_pair>','ifdh_lss_pair','list')
mod.add_container('std::vector<std::string>','std::string','list')
mod.add_container('std::map<std::string,std::vector<std::string> >',(retval('std::string'),retval('std::vector<std::string>')),'map')

klass = mod.add_class("ifdh")
klass.add_constructor([param('std::string','baseuri',default_value='""')])
klass.add_method('set_base_uri',None,[param('std::string','baseuri')])
klass.add_method('set_debug',None,[param('std::string','s')])
klass.add_method('cp',retval('int'), [param('std::vector<std::string>','args')])
klass.add_method('fetchInput',retval('std::string'), [param('std::string','src_uri')])
klass.add_method('localPath',retval('std::string'), [param('std::string','src_uri')])
klass.add_method('addOutputFile',retval('int'), [param('std::string','filename')])
klass.add_method('copyBackOutput',retval('int'), [param('std::string','dest_dir'),param('int','hash',default_value='0')])
klass.add_method('log',retval('int'),[param('std::string','message')])
klass.add_method('enterState',retval('int'),[param('std::string','state')])
klass.add_method('leaveState',retval('int'),[param('std::string','state')])
klass.add_method('createDefinition',retval('int'),[param('std::string','name'),param('std::string','dims'),param('std::string','user'),param('std::string','group')])
klass.add_method('deleteDefinition',retval('int'),[param('std::string','name')])
klass.add_method('describeDefinition',retval('int'),[param('std::string','name')])
klass.add_method('translateConstraints',retval('std::vector<std::string>'),[param('std::string','dims')])
klass.add_method('locateFile',retval('std::vector<std::string>'),[param('std::string','name'),param('std::string','schema',default_value='""')])
klass.add_method('getMetadata',retval('int'),[param('std::string','name')])
klass.add_method('dumpStation',retval('int'),[param('std::string','name'),param('std::string','what',default_value='"all"')])
klass.add_method('startProject',retval('std::string'),[param('std::string','name'),param('std::string','station'),param('std::string','defname_or_id'),param('std::string','user'),param('std::string','group')])
klass.add_method('findProject',retval('std::string'),[param('std::string','name'),param('std::string','station')])
klass.add_method('establishProcess', retval('std::string'), [param('std::string', 'projecturi'), param('std::string', 'appname'), param('std::string', 'appversion'), param('std::string', 'location'), param('std::string', 'user'), param('std::string', 'appfamily', default_value = '""'), param('std::string', 'description', default_value = '""'), param('int', 'filelimit', default_value = '-1'), param('std::string', 'schemas', default_value = '""')])
klass.add_method('getNextFile',retval('std::string'),[param('std::string','projecturi'),param('std::string','processid')])

klass.add_method('updateFileStatus',retval('std::string'),[param('std::string','projecturi'),param('std::string','processid'),param('std::string','filename'),param('std::string','status')])

klass.add_method('endProcess',retval('int'),[param('std::string','projecturi'),param('std::string','processid')])
klass.add_method('endProject',retval('int'),[param('std::string','projecturi')])
klass.add_method('dumpProject',retval('std::string'),[param('std::string','projecturi')])
klass.add_method('setStatus',retval('int'),[param('std::string','projecturi'),param('std::string','processid'),param('std::string','status')])
klass.add_method('cleanup',retval('int'),[])
klass.add_method('renameOutput',retval('int'),[param('std::string','how')])
klass.add_method('mv',retval('int'),[param('std::vector<std::string>','args')])
klass.add_method('ls',retval('std::vector<std::string>'),[param('std::string','loc'), param('int','recursion_depth'), param('std::string','force',default_value='""')])
klass.add_method('mkdir',retval('int'),[param('std::string','loc'), param('std::string', 'force',default_value='""')])
klass.add_method('rm',retval('int'),[param('std::string','loc'), param('std::string', 'force',default_value='""')])
klass.add_method('rmdir',retval('int'),[param('std::string','loc'), param('std::string', 'force',default_value='""')])
klass.add_method('more',retval('int'),[param('std::string','loc')])
klass.add_method('chmod',retval('int'),[param('std::string','mode'),param('std::string','loc'), param('std::string', 'force',default_value='""')])
klass.add_method('rename',retval('int'),[param('std::string','loc'),param('std::string','loc2'), param('std::string', 'force',default_value='""')])
klass.add_method('ll',retval('std::vector<std::string>'),[param('std::string','loc'), param('int','recursion_depth'), param('std::string','force',default_value='""')])
klass.add_method('lss',retval('std::vector<ifdh_lss_pair>'),[param('std::string','loc'), param('int','recursion_depth'), param('std::string','force',default_value='""')])
klass.add_method('findMatchingFiles',retval('std::vector<ifdh_lss_pair>'),[param('std::string','path'), param('std::string','glob')])
klass.add_method('fetchSharedFiles',retval('std::vector<ifdh_lss_pair>'),[param('std::vector<ifdh_lss_pair>','list'),param('std::string','schema',default_value='""')])
klass.add_method('locateFiles',retval('std::map<std::string,std::vector<std::string> >'),[param('std::vector<std::string>','args')])

'''
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
std::string getUrl(std::string loc, std::string force);
std::string getErrorText();
// generate snapshot of a named dataset definition
std::string takeSnapshot( std::string name);
// check if project is alive
std::string projectStatus(std::string projecturi);
private:
IFile lookup_loc(std::string url) ;
std::string locpath(IFile loc, std::string proto) ;
int retry_system(const char *cmd_str, int error_expected,  cpn_lock &locker, ifdh_op_msg &mbuf, int maxtries = -1, std::string unlink_on_error = "") ;
std::string srcpath(CpPair &cpp) ;
std::string dstpath(CpPair &cpp) ;
int do_cp_bg(CpPair &cpp, bool intermed_file_flag, bool recursive, cpn_lock &cpn,  ifdh_op_msg &mbuf);
int do_cp(CpPair &cpp,  bool intermed_file_flag, bool recursive, cpn_lock &cpn, ifdh_op_msg &mbuf) ;
void pick_proto(CpPair &p, std::string force) ;
std::vector<CpPair> handle_args( std::vector<std::string> args, std::vector<std::string>::size_type curarg, bool dest_is_dir,  size_t &lock_low, size_t &lock_hi, std::string &force);
bool have_stage_subdirs(std::string uri);
void pick_proto_path(std::string loc, std::string force, std::string &proto, std::string &fullurl, std::string &lookup_proto );
int do_url_int(int postflag, ...);
std::string do_url_str(int postflag,...);
std::vector<std::string> do_url_lst(int postflag,...);
int addFileLocation(std::string filename, std::string location);
};

}
'''

mod.generate(sys.stdout)
