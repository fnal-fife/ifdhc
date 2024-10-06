#import unittest2 as unittest
import unittest
import socket
import os
import time
import glob
import sys
from tempfile import NamedTemporaryFile
import hypot_test_env


class Skipped(EnvironmentError):
    pass

class aux_file_cases(unittest.TestCase):
    experiment = "hypot"
    tc = 0
    buffer = True

    def log(self,msg):
        try:
            print(msg)
            self.ifdh_handle.log(msg)
        except:
            print("error trying to log %s" % msg)
            print("%s - continuing"%sys.exc_info()[1])
            pass

    def assertEqual(self,a,b,test=None):
        try:
            super(aux_file_cases,self).assertEqual(a,b)
            self.log("PASS %s assertEqual(%s,%s)"%(test,a,b))
        except:
            self.log("FAIL %s  assertEqual(%s,%s)"%(test,a,b))
            raise

    def setUp(self):
        hypot_test_env.hypot_test_env()
        import ifdh
        print("-----setup----")
        os.environ['EXPERIMENT'] =  aux_file_cases.experiment
        self.ifdh_handle = ifdh.ifdh()
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
        print("-----end setup----")

    def tearDown(self):
        os.system("rm -rf %s" % self.work)
        pass

    def test_0_matching(self):
        self.log(self._testMethodName)
        l = self.ifdh_handle.findMatchingFiles("%s/a/b:%s/a/c" % (self.work,self.work),"f*")
        print("len:", len(l))
        self.assertEqual(len(l),40, self._testMethodName)

    def test_1_fetch(self):
        l = self.ifdh_handle.findMatchingFiles("%s/a/b:%s/a/c" % (self.work,self.work),"f*")
        l2 = self.ifdh_handle.fetchSharedFiles(l, "")
        print("fetched: %s" % repr(l2))
        self.ifdh_handle.cleanup()
        self.assertEqual(len(l2),40, self._testMethodName)
         
def suite():
    suite =  unittest.TestLoader().loadTestsFromTestCase(aux_file_cases)
    return suite

if __name__ == '__main__':
    unittest.main()

