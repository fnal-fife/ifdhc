#import unittest2 as unittest
import unittest
import ifdh
import socket
import os
import time
import sys

#
# basically make sure copying and ls-ing nonexistent directories fails
# and existing ones succeeds, plain, and in different forced protocols
#

class retrycases(unittest.TestCase):


    def log(self,msg):
        try:
            #print msg
            self.ifdh_handle.log(msg)
        except:
            print "error trying to log %s" % msg
            print "%s - continuing"%sys.exc_info()[1]
            pass


    def assertNotEqual(self,a,b,test=None,note=None):
        if test == None:
            test = self._testMethodName
        if note is not None:
            test ="%s %s"%(test, note)
        try:
            unittest.TestCase.assertNotEqual(self,a,b,test)
            self.log("PASS %s assertNotEqual(%s,%s)"%(test,a,b))
        except:
            self.log("FAIL %s  assertNotEqual(%s,%s)"%(test,a,b))
            raise

    def assertEqual(self,a,b,test=None,note=None):
        if test == None:
            test = self._testMethodName
        if note is not None:
            test ="%s %s"%(test, note)
        try:
            unittest.TestCase.assertEqual(self,a,b,test)
            self.log("PASS %s assertEqual(%s,%s)"%(test,a,b))
        except:
            self.log("FAIL %s  assertEqual(%s,%s)"%(test,a,b))
            raise

    def setUp(self):
        sys.stdout.flush()
        sys.stderr.flush()
        self.ifdh_handle = ifdh.ifdh()
        self.errfile = "/tmp/err%d" % os.getpid()
        try:
            os.mkdir('empty')
        except:
            pass
        sys.stdout.flush()
        self.exp = 'nova'

    def tearDown(self):
        pass

    def check_no_retries(self):
        found = False
        toolong = True
        with open(self.errfile,'r') as f:
            for line in f:
                if line == 'retrying...\n':
                     found = True
                     print "saw: ", line, "bad: should not have retried"
                if line[:4] == 'real':
                     toolong = line[5:9] != '0m0.'
                     if toolong:
                         print "saw: ", line, "bad: too long!"
        self.assertEqual(found, False)
        self.assertEqual(toolong, False)

    def test_star_cp(self):
        os.environ['IFDH_CP_MAXRETRIES'] = "1"
        res = os.system("(time ifdh cp -D empty/* /tmp)2>%s" % self.errfile)
        self.check_no_retries()

    def test_max_retries(self):
        os.environ['IFDH_CP_MAXRETRIES'] = "0"
        res = os.system("(time ifdh cp nosuchfile /tmp/nosuchfile )2>%s" % self.errfile)
        self.check_no_retries()

def suite():
    suite =  unittest.TestLoader().loadTestsFromTestCase(retrycases)
    return suite

if __name__ == '__main__':
    unittest.main()
