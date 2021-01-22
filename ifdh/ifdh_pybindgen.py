
import pybindgen
import sys
from pybindgen import Module, retval, param

#
# Note -- this currently requires my fork of pybindgen, which
#   supportst the auto_convert option to add_container...
#

mod=pybindgen.Module("ifdh",cpp_namespace="ifdh_ns")
mod.add_include('"ifdh.h"')

std_logicerror = mod.add_exception('logic_error',foreign_cpp_namespace='std',message_rvalue="%(EXC)s.what()")

pair = mod.add_struct('ifdh_lss_pair')
pair.add_instance_attribute('first','std::string')
pair.add_instance_attribute('second','long')

mod.add_container('std::vector<ifdh_lss_pair>','ifdh_lss_pair','vector',auto_convert="PyList_Type")
mod.add_container('std::vector<std::string>','std::string','vector',auto_convert="PyTuple_Type")
mod.add_container('std::map<std::string,std::vector<std::string> >',['std::string','std::vector<std::string>'],'map')

klass = mod.add_class("ifdh")
klass.add_constructor([param('std::string','baseuri',default_value='""')])
klass.add_method('set_debug',None,[param('std::string','s')],throw=[std_logicerror])
klass.add_method('set_base_uri',None,[param('std::string','baseuri')],throw=[std_logicerror])
klass.add_method('cp',retval('int'), [param('std::vector<std::string>','args')],throw=[std_logicerror])
klass.add_method('fetchInput',retval('std::string'), [param('std::string','src_uri')],throw=[std_logicerror])
klass.add_method('localPath',retval('std::string'), [param('std::string','src_uri')],throw=[std_logicerror])
klass.add_method('addOutputFile',retval('int'), [param('std::string','filename')],throw=[std_logicerror])
klass.add_method('copyBackOutput',retval('int'), [param('std::string','dest_dir'),param('int','hash',default_value='0')],throw=[std_logicerror])
klass.add_method('log',retval('int'),[param('std::string','message')],throw=[std_logicerror])
klass.add_method('enterState',retval('int'),[param('std::string','state')],throw=[std_logicerror])
klass.add_method('leaveState',retval('int'),[param('std::string','state')],throw=[std_logicerror])
klass.add_method('createDefinition',retval('int'),[param('std::string','name'),param('std::string','dims'),param('std::string','user'),param('std::string','group')],throw=[std_logicerror])
klass.add_method('deleteDefinition',retval('int'),[param('std::string','name')],throw=[std_logicerror])
klass.add_method('describeDefinition',retval('std::string'),[param('std::string','name')],throw=[std_logicerror])
klass.add_method('translateConstraints',retval('std::vector<std::string>'),[param('std::string','dims')],throw=[std_logicerror])
klass.add_method('locateFile',retval('std::vector<std::string>'),[param('std::string','name'),param('std::string','schema',default_value='""')],throw=[std_logicerror])
klass.add_method('getMetadata',retval('std::string'),[param('std::string','name')],throw=[std_logicerror])
klass.add_method('dumpStation',retval('std::string'),[param('std::string','name'),param('std::string','what',default_value='"all"')],throw=[std_logicerror])
klass.add_method('startProject',retval('std::string'),[param('std::string','name'),param('std::string','station'),param('std::string','defname_or_id'),param('std::string','user'),param('std::string','group')],throw=[std_logicerror])
klass.add_method('findProject',retval('std::string'),[param('std::string','name'),param('std::string','station')],throw=[std_logicerror])
klass.add_method('establishProcess', retval('std::string'), [param('std::string', 'projecturi'), param('std::string', 'appname'), param('std::string', 'appversion'), param('std::string', 'location'), param('std::string', 'user'), param('std::string', 'appfamily', default_value = '""'), param('std::string', 'description', default_value = '""'), param('int', 'filelimit', default_value = '-1'), param('std::string', 'schemas', default_value = '""')],throw=[std_logicerror])
klass.add_method('getNextFile',retval('std::string'),[param('std::string','projecturi'),param('std::string','processid')],throw=[std_logicerror])

klass.add_method('updateFileStatus',retval('std::string'),[param('std::string','projecturi'),param('std::string','processid'),param('std::string','filename'),param('std::string','status')],throw=[std_logicerror])

klass.add_method('endProcess',retval('int'),[param('std::string','projecturi'),param('std::string','processid')],throw=[std_logicerror])
klass.add_method('endProject',retval('int'),[param('std::string','projecturi')],throw=[std_logicerror])
klass.add_method('dumpProject',retval('std::string'),[param('std::string','projecturi')],throw=[std_logicerror])
klass.add_method('setStatus',retval('int'),[param('std::string','projecturi'),param('std::string','processid'),param('std::string','status')],throw=[std_logicerror])
klass.add_method('cleanup',retval('int'),[],throw=[std_logicerror])
klass.add_method('renameOutput',retval('int'),[param('std::string','how')],throw=[std_logicerror])
klass.add_method('mv',retval('int'),[param('std::vector<std::string>','args')],throw=[std_logicerror])
klass.add_method('ls',retval('std::vector<std::string>'),[param('std::string','loc'), param('int','recursion_depth'), param('std::string','force',default_value='""')],throw=[std_logicerror])
klass.add_method('mkdir',retval('int'),[param('std::string','loc'), param('std::string', 'force',default_value='""')],throw=[std_logicerror])
klass.add_method('rm',retval('int'),[param('std::string','loc'), param('std::string', 'force',default_value='""')],throw=[std_logicerror])
klass.add_method('rmdir',retval('int'),[param('std::string','loc'), param('std::string', 'force',default_value='""')],throw=[std_logicerror])
klass.add_method('more',retval('int'),[param('std::string','loc')],throw=[std_logicerror])
klass.add_method('chmod',retval('int'),[param('std::string','mode'),param('std::string','loc'), param('std::string', 'force',default_value='""')],throw=[std_logicerror])
klass.add_method('rename',retval('int'),[param('std::string','loc'),param('std::string','loc2'), param('std::string', 'force',default_value='""')],throw=[std_logicerror])
klass.add_method('ll',retval('int'),[param('std::string','loc'), param('int','recursion_depth'), param('std::string','force',default_value='""')],throw=[std_logicerror])
klass.add_method('lss',retval('std::vector<ifdh_lss_pair>'),[param('std::string','loc'), param('int','recursion_depth'), param('std::string','force',default_value='""')],throw=[std_logicerror])
klass.add_method('findMatchingFiles',retval('std::vector<ifdh_lss_pair>'),[param('std::string','path'), param('std::string','glob')],throw=[std_logicerror])
klass.add_method('fetchSharedFiles',retval('std::vector<ifdh_lss_pair>'),[param('std::vector<ifdh_lss_pair>','list'),param('std::string','schema',default_value='""')],throw=[std_logicerror])
klass.add_method('locateFiles',retval('std::map<std::string,std::vector<std::string> >'),[param('std::vector<std::string>','args')],throw=[std_logicerror])
klass.add_method('checksum',retval('std::string'),[param('std::string','loc')],throw=[std_logicerror])
klass.add_method('mkdir_p',retval('int'),[param('std::string','loc'), param('std::string', 'force',default_value='""'),param('int','depth',default_value='-1')],throw=[std_logicerror])
klass.add_method('getProxy',retval('std::string'),[],throw=[std_logicerror])
klass.add_method('getToken',retval('std::string'),[],throw=[std_logicerror])
klass.add_method('declareFile',retval('int'),[param('std::string','json_metadata')],throw=[std_logicerror])
klass.add_method('modifyMetadata',retval('int'),[param('std::string','file'),param('std::string','json_metadata')],throw=[std_logicerror])
klass.add_method('apply',retval('int'),[param('std::vector<std::string>','args')],throw=[std_logicerror])
klass.add_method('getUrl',retval('std::string'),[param('std::string','loc'), param('std::string', 'force',default_value='""')],throw=[std_logicerror])
klass.add_method('getErrorText',retval('std::string'),[],throw=[std_logicerror])
klass.add_method('takeSnapshot',retval('std::string'),[param('std::string','name')],throw=[std_logicerror])
klass.add_method('projectStatus',retval('std::string'),[param('std::string','projecturi')],throw=[std_logicerror])
klass.add_method('addFileLocation',retval('int'),[param('std::string','filename'),param('std::string','location')],throw=[std_logicerror])


mod.generate(sys.stdout)
