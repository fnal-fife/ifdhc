import unittest
import ifdh
import socket
import os
import time
import glob
import sys
from tempfile import NamedTemporaryFile


base_uri_fmt = "http://samweb.fnal.gov:8480/sam/%s/api"

class Skipped(EnvironmentError):
    pass

class aux_file_cases(unittest.TestCase):
    experiment = "nova"
    tc = 0
    buffer = True

    def log(self,msg):
        try:
            print msg
            self.ifdh_handle.log(msg)
        except:
            print "error trying to log %s" % msg
            print "%s - continuing"%sys.exc_info()[1]
            pass

    def mk_remote_dir(self,dir,opts=''):
        try:
            os.system('uberftp -mkdir "gsiftp://fg-bestman1.fnal.gov:2811%s" > /dev/null 2>&1' % (dir))
        except:
            pass
        try:
            os.system('uberftp -chmod 775 "gsiftp://fg-bestman1.fnal.gov:2811%s" > /dev/null 2>&1' % (dir))
        except:
            pass

    def assertEqual(self,a,b,test=None):
        try:
            super(aux_file_cases,self).assertEqual(a,b)
            self.log("PASS %s assertEqual(%s,%s)"%(test,a,b))
        except:
            self.log("FAIL %s  assertEqual(%s,%s)"%(test,a,b))
            raise


    def dirlog(self,msg):
        self.log(msg)
        self.mk_remote_dir('%s/%s'%(self.data_dir,msg))

    def setUp(self):
        os.environ['EXPERIMENT'] =  aux_file_cases.experiment
        self.ifdh_handle = ifdh.ifdh(base_uri_fmt % aux_file_cases.experiment)
        self.hostname = socket.gethostname()
        self.work="%s/work%d" % (os.environ.get('TMPDIR','/tmp'),os.getpid())
	self.data_dir_root="/grid/data/%s/%s" % (os.environ.get('TEST_USER', os.environ['USER']), self.hostname)
	self.data_dir="/grid/data/%s/%s/%s" % (os.environ.get('TEST_USER', os.environ['USER']), self.hostname,os.getpid())
        
        # setup test directory tree..
        count = 0
        os.mkdir("%s" % (self.work))
        for sd in [ 'a', 'a/b', 'a/c' ]:
            os.mkdir("%s/%s" % (self.work,sd))
            for i in range(20):
                count = count+1
                f = open("%s/%s/f%d" % (self.work,sd,count),"w")
                f.write("foo\n")
                f.close()
        os.system("ls -R %s" % self.work)

    def tearDown(self):
        os.system("rm -rf %s" % self.work)
        pass

    # somehow this test breaks the others later on(?)
    def test_0_matching(self):
        self.log(self._testMethodName)
        l = self.ifdh_handle.findMatchingFiles("%s/a/b:%s/a/c" % (self.work,self.work),"f*")
        self.assertEqual(len(l),40, self._testMethodName)

    def test_1_fetch(self):
        l = self.ifdh_handle.findMatchingFiles("%s/a/b:%s/a/c" % (self.work,self.work),"f*")
        l2 = self.ifdh_handle.fetchSharedFiles(l, "")
        self.ifdh_handle.cleanup()
        self.assertEqual(len(l2),40, self._testMethodName)
         
def suite():
    suite =  unittest.TestLoader().loadTestsFromTestCase(aux_file_cases)
    return suite

if __name__ == '__main__':
    unittest.main()

