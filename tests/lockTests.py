#!/usr/bin/env python

import unittest
import ifdh
import socket
import os
import time
import glob
from subprocess import Popen, PIPE
import stat
import sys

base_uri_fmt = "http://samweb.fnal.gov:8480/sam/%s/api"


class ifdh_lock_cases(unittest.TestCase):

    buffer = True

    def set_limit_wait(self, l, w):
        f = open(self.limit,"w")
        f.write("%d\n" % l)
        f.close()
        f = open(self.wait,"w")
        f.write("%d\n" % w)
        f.close()

    def setUp(self):
        """
           private lock stuff, based on:
           https://cdcvs.fnal.gov/redmine/projects/ifdhcpn/wiki/DEVELOPMENT#Move-to-private-test-LOCKs
        """
        self.lockbase = '/nusoft/app/home'
        self.lockgroup = 'ifmon'
        self.lock = '%s/%s/LOCK' % (self.lockbase, self.lockgroup)
        # put CPN_DIR back if cpTests hid it...
        if os.environ.has_key('SAVE_CPN_DIR'):
            os.environ['CPN_DIR'] = os.environ['SAVE_CPN_DIR']
        os.environ['CPN_DIR'] = '/no/such/dir'
        os.environ['CPN_LOCK_BASE'] = self.lockbase
        os.environ['CPN_LOCK_GROUP'] = self.lockgroup
        self.limit = '%s/limit' % self.lock
        self.wait = '%s/wait' % self.lock
        self.ifdh = ifdh.ifdh()
        # always put stats back...
        self.set_limit_wait(5,5)

    def tearDown(self):
        # always put stats back...
        self.set_limit_wait(5,5)
        #os.system("lock clean")

    def start_copy(self, source="/dev/null"):

        if source == "/dev/null":
            dest = "/grid/data/mengel/.empty"
        else:
            dest = "/dev/null"

        self.cp_s = Popen("exec ifdh cp --force=cpn %s %s 2>&1" % (source, dest),shell=True, bufsize=1024, stdout=PIPE, preexec_fn=os.setsid)
        self.cp_p = self.cp_s.stdout

    def get_lock_line(self):
        s = self.cp_p.readline()
        print "\nread line: ", s
        while s != ""  and (not ("LOCK -" in s) or ("already in progress" in s)):
            s = self.cp_p.readline()
            print "\nread line: ", s
        return s

 
    def check_heartbeat(self,qf):
        try:
            sb = os.stat(qf)
        except OSError:
            return False

        t1 = sb[stat.ST_MTIME]
        print "time before: " , t1
        for i in range(0,7):
            sys.stdout.write(".")
            sys.stdout.flush()
            time.sleep(60)
        print ""
        try:
            sb = os.stat(qf)
        except OSError:
            return False
 
        t2 = sb[stat.ST_MTIME]
        print "time after: " , t2
        return t1 != t2

    def wakeup(self):
        # guess port -- it should be our ifdh_cp's  pid + 2000
        portnum = self.cp_s.pid  + 2000
    
        #os.system("ps --forest ");
        #print "trying port ", portnum

        command = "echo wakeup > /dev/udp/`hostname`/%d" % portnum
        #print "trying: ", command
        os.system( command )

    def expect_lock_line(self,  what, what2 = None, what3 = None ):
        print "checking for '%s' ..." % what
        s = self.get_lock_line()
        print "...in '%s'" %  s
        if what2:
            while what2 in s or what3 in s:
                print "skipping because of %s, %s" % (what2, what3)
                s = self.get_lock_line()
	        print "...in '%s'" %  s
        self.assertTrue(what in s)
        return s

    def test_01_simple_lock(self):
        """
           https://cdcvs.fnal.gov/redmine/projects/ifdhcpn/wiki/DEVELOPMENT#Simple-lock-and-release
           but we do an empty copy to do the lock/unlock
        """
        self.start_copy()
        self.expect_lock_line(" lock ")
        self.expect_lock_line(" freed ")

        f = Popen("ls -l %s/LOG" % self.lock, shell=True, bufsize=1024, stdout=PIPE).stdout
        for line in f.readlines():
            last = line
        f.close()
        self.assertTrue(os.environ['USER'] in last)

    def test_02_single_queued(self):
        # set param files to force waiting...
        self.set_limit_wait(0,200)

        # start a copy which will sleep
        self.start_copy()
        s = self.expect_lock_line(" sleeping ")
        s = self.expect_lock_line(" queue ")

        # make sure queue file exists, and has time updated
        p = s.find("queue")
        print "checking queue..."
        qf = "%s/QUEUE/%s" % ( self.lock , s[p+6:-1])

        self.assertTrue(self.check_heartbeat(qf))

        # set parameters to let it continue
        self.set_limit_wait(5,5)

        self.wakeup()

        self.expect_lock_line(" lock ", " queue ", " sleeping ")
        self.expect_lock_line(" freed ")

    def test_03_interrupt_queued(self):
        # set param files to force waiting...
        self.set_limit_wait(0,200)

        # start a copy which will sleep
        self.start_copy()
        s = self.expect_lock_line(" sleeping ")
        s = self.expect_lock_line(" queue ")

        os.killpg( self.cp_s.pid, 15)
        time.sleep(5)
        
        # make sure queue file exists, and has time updated
        p = s.find("queue")
        print "checking queue..."
        qf = "%s/QUEUE/%s" % ( self.lock , s[p+6:-1])

        self.assertFalse(self.check_heartbeat(qf))
        self.set_limit_wait(5,5)


    def test_04_interrupted_copy(self):

        # start a copy which will sleep
        self.start_copy("/grid/data/ifmon/CPNTEST/FILES/20G")
        s = self.expect_lock_line(" lock ")

       
        os.killpg( self.cp_s.pid, 15)

        lockf = s[s.rfind(" ")+1:-1]
        print "checking lock file: '%s'" % lockf
        
        self.assertFalse(self.check_heartbeat(lockf))
        
    def test_05_interrupted_just_cp(self):

        # start a copy which will sleep
        self.start_copy("/grid/data/ifmon/CPNTEST/FILES/20G")
        s = self.expect_lock_line(" lock ")

        # kill off just the underlying cp...
        time.sleep(1)
        cmd = "ps -ef | grep -v grep | grep ' %d ' | egrep ' cp /| dd.*if./' " % self.cp_s.pid 
        print "looking with: " , cmd
        f = os.popen(cmd , "r")
        pids = f.read()
        f.close()
        print "found pid: ", pids
        print "trimmed pid: ", pids[7:14]

        os.kill( int(pids[7:14]), 9)

        # wait long enough for the background task to notice...
        time.sleep(65)

        lockf = s[s.rfind(" ")+1:-1]
        print "checking lock file: '%s'" % lockf
        
        self.assertFalse(self.check_heartbeat(lockf))
        
def suite():
    suite =  unittest.TestLoader().loadTestsFromTestCase(ifdh_lock_cases)
    return suite

if __name__ == '__main__':
    unittest.main()
