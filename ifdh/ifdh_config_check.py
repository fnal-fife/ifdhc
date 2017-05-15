#!/usr/bin/env python

try:
    from configparser import RawConfigParser

    class mycf(RawConfigParser):
        def get3(self, s, t, default = None):
            return self.get(s,t,default)
except:

    from ConfigParser import RawConfigParser

    class mycf(RawConfigParser):
        def get3(self, s, t, default = None):
            try:
                return self.get(s,t)
            except:
                return default


class IFDHConfigError(Exception):
    """Base class for exceptions in IFDH."""
    type = "IFDHConditionalError"


class IFDHConditionalError(IFDHConfigError):
    def __init__(self, msg):
        #self.expr = expr
        self.type = "IFDHConditionalError"
        self.msg = msg

class IFDHRotationError(IFDHConfigError):
    def __init__(self, msg):
        #self.expr = expr
        self.type = "IFDHRotationError"
        self.msg = msg

class IFDHVOError(IFDHConfigError):
    def __init__(self, msg):
        #self.expr = expr
        self.type = "IFDHVOError"
        self.msg = msg

class IFDHPrefixError(IFDHConfigError):
    def __init__(self, msg):
        #self.expr = expr
        self.type = "IFDHPrefixError"
        self.msg = msg

class IFDHLocationError(IFDHConfigError):
    def __init__(self, msg):
        #self.expr = expr
        self.type = "IFDHLocationError"
        self.msg = msg

class IFDHProtocolError(IFDHConfigError):
    def __init__(self, msg):
        #self.expr = expr
        self.type = "IFDHProtocolError"
        self.msg = msg

class IFDHExpansionError(IFDHConfigError):
    def __init__(self, msg):
        #self.expr = expr
        self.type = "IFDHExpansionError"
        self.msg = msg



import re
import os
import sys

def verify(x, name, section,  *args):
    if not x:
        error_type = section.split(' ', 1)[0]
        if error_type == "conditional":
            raise IFDHConditionalError("Error: %s in [%s] %s \n" % (name, section, ' '.join(args)))
        elif error_type == "rotation":
            raise IFDHRotationError("Error: %s in [%s] %s \n" % (name, section, ' '.join(args)))
        elif error_type == "experiment_vo":
            raise IFDHVOError("Error: %s in [%s] %s \n" % (name, section, ' '.join(args)))
        elif error_type == "prefix":
            raise IFDHPrefixError("Error: %s in [%s] %s \n" % (name, section, ' '.join(args)))
        elif error_type == "location":
            raise IFDHLocationError("Error: %s in [%s] %s \n" % (name, section, ' '.join(args)))
        elif error_type == "protocol":
            raise IFDHProtocolError("Error: %s in [%s] %s \n" % (name, section, ' '.join(args)))
        elif error_type == "expansion":
            raise IFDHExpansionError('expansion %s needs definition' % name)
        else:
            raise IFDHConfigError

def check_conditionals(cp):
    print "\tChecking conditionals..."
    clist = cp.get3('general','conditionals').split(' ')
    for c in clist:
        if c == '':
             continue
        s = 'conditional %s' % c
        t = cp.get3(s, 'test', None)
        a1 = cp.get3(s, 'rename_proto', None)
        a2 = cp.get3(s, 'rename_loc', None)
        verify(t and (a1 or a2), 'Conditional needs test and command', s)

def check_rotations(cp):
    print "\tChecking rotations..."
    rlist = cp.get3('general','rotations').split(' ')
    for r in rlist:
        if r == '':
            continue
        s = 'rotation %s' % r
        dp = cp.get3(s, 'door_proto', None)
        drre = cp.get3(s, 'door_repl_re', None)
        du = cp.get3(s,'lookup_door_uri', None)
        dd = cp.get3(s,'default_doors', None)
        verify( dp and drre and du and dd, 'Rotations need door_proto, door_repl_re,lookup_door_uri and default_doors', s)

def check_vo_rules(cp):
    print "\tChecking virtual organizations..."
    s = 'experiment_vo'
    n = int(cp.get3(s,'numrules', 0))
    if n == 0:
        raise IFDHVOError("Please specify the field numrules for [experiment_vo]")
    for i in range(1,n+1):
        verify(cp.get3(s,'match%d' % i, None) and cp.get3(s,'repl%d' % i, None),'experiment_vo needs match_i and repl_i tags for all i in 1..numrules',s)

def check_percents(cp, filetext):
<<<<<<< HEAD
    print "\tChecking expansions..."
=======
>>>>>>> 9fd3841382a88b966d0626f3b47dfac03d3aeea0
    l1  = re.findall('\%\([a-z0-9-_]*\)', filetext)
    s1 = set(['%(src)','%(dst)', '%(r)', '%(dst)', '%(extra)','%(recursion_flag)','%(recursion_depth)','%(mode)','%(obits)','%(gbits)','%(secs)'])
    ml = cp.options('macros')

    print "found list of expansions:", l1
    for m in ml:
        s1.add('%%(%s)s' % m)

    for s in l1:
        verify(s in s1, 'expansion %s needs definition' % s, 'expansion')

def check_prefixes(cp):
    print "\tChecking prefixes..."
    plist = cp.get3('general','prefixes').split(' ')
    locations = set()
    for p in plist:
        if p == '':
            continue
        s = 'prefix %s' % p
        loc = cp.get3(s, 'location', None)
        if loc:
            locations.add(loc)
        ss = cp.get3(s, 'slashstrip', None)
        loctag = cp.get3('location %s' % loc, 'protocols', None)
        verify(loc and ss, 'Prefixes need location and slashstrip', s)
        verify(loctag, 'Locations need to be defined', s)

    for s in cp.sections():
        if s.startswith('prefix '):
           prefix = s[7:]
           verify(prefix in plist, 'prefixes need to be in general.prefixes list', s)
        #
        # locations referenced in rename_loc are referenced, so add them
        # to the set while we're here...
        #
        if s.startswith('conditional'):
           l = cp.get3(s, 'rename_loc', '').split(' ')
           if len(l) and l[0]:
               locations.add(l[0])

    return locations

def check_locations(cp, locations):
    print "\tChecking locations..."
    for l in locations:
        s = 'location %s' % l

        for t in ['need_cpn_lock','protocols'] + ['prefix_%s'%x[:-1] for x in cp.get3(s,'protocols','').split(' ')]:
            if t == 'prefix_':
                continue
            verify(cp.get3(s,t,None) != None, 'Location missing tag', s, t)

    for s in cp.sections():
        if s.startswith('location '):
           loc = s[9:]
           verify(loc in locations, 'Locations need to be referenced',s, loc)

def check_protocols(cp):
    print "\tChecking protocols..."
    plist = cp.get3('general','protocols').split(' ')
    for p in plist:
        if p == '':
            continue
        s = 'protocol %s' % p
        aa = cp.get3(s,'alias', None)
        if ( aa ):
            # make sure there's some there there...
            verify(cp.get3('protocol %s' % aa,'cp_cmd',None), 'Protocol aliases need to refer to existing protocols', s)
        else:
            for t in ['extra_env','cp_cmd','cp_r_cmd','lss_cmd','lss_skip','lss_size_last','lss_dir_last','lss_re1','ll_cmd','mv_cmd','chmod_cmd','rm_cmd','rmdir_cmd','pin_cmd']:
                verify(cp.get3(s,t,None) != None, 'Protocol missing tag:', s, t)

    for s in cp.sections():
        if s.startswith('protocol '):
           p = s[9:]
           verify(p in plist, 'protocol not in general.protocols list', s)

def do_check(filename):
    cp = mycf()
    f = open(filename,"r")
    filetext = f.read()
    f.close()
    cp.read(filename)
    # list from Feature issue #16503
    check_conditionals(cp)
    check_rotations(cp)
    check_vo_rules(cp)
    check_percents(cp, filetext)
    loclist = check_prefixes(cp)
    check_locations(cp, loclist) # handles several items
    check_protocols(cp) # several items

if __name__ == '__main__':
    print "Checking the configuration file...."
    try:
        do_check(os.environ['IFDHC_CONFIG_DIR']+'/ifdh.cfg')
    except (IFDHConfigError, IFDHConditionalError, IFDHRotationError, IFDHVOError, IFDHPrefixError, IFDHLocationError, IFDHProtocolError, IFDHExpansionError) as ex:
        print "Exception has been raised: ", ex.type
        print ex.msg
        sys.exit(2)
    print "Configuration is acceptable"
