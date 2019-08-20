import unittest
import ifdh
import socket
import os
import time
import sys

#
# flag to use development sam instances
#
do_dev_sam = False

if do_dev_sam:
    base_uri_fmt = "http://samweb.fnal.gov:8480/sam/%s/dev/api"
else:
    base_uri_fmt = "http://samweb.fnal.gov:8480/sam/%s/api"


class TimeoutCases(unittest.TestCase):

    def setUp(self):
        self.hostname = socket.gethostname()

    def regular(self):
        self.ifdh_handle = ifdh.ifdh(base_uri_fmt % "samdev")

    def slow(self):
        self.ifdh_handle = ifdh.ifdh("http://www.deelay.me/10000/" + base_uri_fmt % "samdev")
          
    def tearDown(self):
        self.ifdh_handle.cleanup()
        self.ifdh_handle = None

    def log(self,msg):
        self.ifdh_handle.log(msg)
        print(msg)

    def assertEqual(self,a,b,test=None):
        try:
            super(TimeoutCases,self).assertEqual(a,b)
            self.log("PASS %s assertEqual(%s,%s)"%(test,a,b))
        except Exception as e:
            self.log("FAIL %s  assertEqual(%s,%s)"%(test,a,b))
            raise e

    def assertNotEqual(self,a,b,test=None):
        try:
            super(TimeoutCases,self).assertNotEqual(a,b)
            self.log("PASS %s assertNotEqual(%s,%s)"%(test,a,b))
        except:
            self.log("FAIL %s  assertNotEqual(%s,%s)"%(test,a,b))
            raise


    def test1(self):
        self.regular()
        res = self.ifdh_handle.locateFile("nosuchfile")
        self.assertEqual(res,(), "t1")

    def test2(self):
        self.slow()
        t1 = time.time()
        res = self.ifdh_handle.locateFile("nosuchfile")
        t2 = time.time()
        os.environ['IFDH_WEB_TIMEOUT'] = '5'
        res = self.ifdh_handle.locateFile("nosuchfile")
        t3 = time.time()
           
        print("try 1", t2 - t1, "try 2" , t3 - t2)
        self.assertEqual(int(t2 -t1), 10, "test2-1")
        self.assertEqual(int(t3 -t2), 5, "test2-2")


if __name__ == '__main__':
    unittest.main()
