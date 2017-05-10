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

import re
import os
import sys

def verify(x, name, section,  *args):
    if not x:
        sys.stderr.write("Error: %s in [%s] %s \n" % (name, section, ' '.join(args)))

def check_conditionals(cp):
    clist = cp.get3('general','conditionals').split(' ')
    for c in clist:
        s = 'conditional %s' % c
        t = cp.get3(s, 'test', None)
        a1 = cp.get3(s, 'rename_proto', None)
        a2 = cp.get3(s, 'rename_loc', None)
        verify(t and (a1 or a2), 'check_conditionals', s)

def check_rotations(cp):
    rlist = cp.get3('general','rotations').split(' ')
    for r in rlist:
        s = 'rotation %s' % r
        dp = cp.get3(s, 'door_proto', None)
        drre = cp.get3(s, 'door_repl_re', None)
        du = cp.get3(s,'lookup_door_uri', None)
        dd = cp.get3(s,'default_doors', None)
        verify(dp and drre and du and dd,'check_rotations', r)

def check_vo_rules(cp):
    s = 'experiment_vo'
    n = int(cp.get3(s,'numrules', 0))
    for i in range(1,n+1):
        verify(cp.get3(s,'match%d' % i, None) and cp.get3(s,'repl%d' % i, None),'check_vo_rules',s)

def check_percents(cp, filetext):
    l1  = re.findall('%([a-z0-9-_])', filetext)
    s1 = set(['%(src)','%(dst)', '%(r)', '%(dst)'])
    ml = cp.options('macros')
    for m in ml:
        s1.add(m)

    for s in l1:
        verify(s in s1, 'check_percents', l1)
    
def check_prefixes(cp):
    plist = cp.get3('general','prefixes').split(' ')
    locations = set()
    for p in plist:
        s = 'prefix %s' % p
        loc = cp.get3(s, 'location', None)
        locations.add(loc)
        ss = cp.get3(s, 'slashstrip', None)
        loctag = cp.get3('location %s' % loc, 'protocols', None)
        verify(loc and ss and loctag, 'check_prefixes', s)

    for s in cp.sections():
        if s.startswith('prefix '):
           prefix = s[7:]
           verify(prefix in plist, 'check_prefixes_2', s)

    return locations

def check_locations(cp, locations):
    for l in locations:
        s = 'location %s' % l
        
        for t in ['need_cpn_lock','protocols'] + ['prefix_%s'%x[:-1] for x in cp.get3(s,'locations','').split(' ')]:
            verify(cp.get3(s,t,None), 'check_protocols_2', s, t)

    for s in cp.sections():
        if s.startswith('location '):
           loc = s[7:]
           verify(loc in locations, 'check_locations_2',s)

def check_protocols(cp):
    plist = cp.get3('general','protocols').split(' ')
    for p in plist:
        s = 'protocol %s' % p
        aa = cp.get3(s,'alias', None)
        if ( aa ):
            # make sure there's some there there...
            verify(cp.get3('protocol %s' % aa,'cp_cmd',None), 'check_protocols_1', s)
        else:
            for t in ['extra_env','cp_cmd','cp_r_cmd','lss_cmd','lss_skip','lss_size_last','lss_dir_last','lss_re1','ll_cmd','mv_cmd','chmod_cmd','rm_cmd','rmdir_cmd','pin_cmd']:
                verify(cp.get3(s,t,None), 'check_protocols_2', s, t)

    for s in cp.sections():
        if s.startswith('protocol '):
           p = s[9:]
           verify(p in plist, 'check_protocols_3', s)

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
    do_check(os.environ['IFDHC_CONFIG_DIR']+'/ifdh.cfg')
