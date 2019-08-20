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

class argparsecases(unittest.TestCase):


    def log(self,msg):
        try:
            #print(msg)
            self.ifdh_handle.log(msg)
        except:
            print("error trying to log %s" % msg)
            print("%s - continuing"%sys.exc_info()[1])
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
        os.environ['IFDH_CP_MAXRETRIES'] = "0"
        sys.stdout.flush()
        self.exp = 'nova'

    def tearDown(self):
        pass

    def test_plain_D(self):
        res = os.system("ifdh cp -D argParse.py /tmp")
        os.unlink("/tmp/argParse.py")
        self.assertEqual(res,0)

    def test_user_D(self):
        res = os.system("ifdh cp --user=fred -D argParse.py /tmp")
        os.unlink("/tmp/argParse.py")
        self.assertEqual(res,0)

def suite():
    suite =  unittest.TestLoader().loadTestsFromTestCase(argparsecases)
    return suite

if __name__ == '__main__':
    unittest.main()


