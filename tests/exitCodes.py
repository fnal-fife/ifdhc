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

class exitcodecases(unittest.TestCase):

    def log(self,msg):
        try:
            print msg
            self.ifdh_handle.log(msg)
        except:
            print "error trying to log %s" % msg
            print "%s - continuing"%sys.exc_info()[1]
            pass

    def assertEqual(self,a,b,test=None):
        if test == None:
            test = self._testMethodName
        try:
            unittest.TestCase.assertEqual(self,a,b)
            self.log("PASS %s assertEqual(%s,%s)"%(test,a,b))
        except:
            self.log("FAIL %s  assertEqual(%s,%s)"%(test,a,b))
            raise

    def setUp(self):
        self.ifdh_handle = ifdh.ifdh()

    def test_ls_noexist(self, force="", experiment="nova"):
        res = os.system("EXPERIMENT=%s ifdh ls /pnfs/nova/scratch/users/nosuchuser 0 %s> /dev/null 2>&1" % (experiment, force))

    def test_ls_noexist_gridftp(self):
        self.test_ls_noexist(force="--force=gridftp")

    def test_ls_noexist_srm(self):
        self.test_ls_noexist(force="--force=srm")

    def test_ls_noexist_local(self):
        res = os.system("ifdh ls /tmp/nosuchdir 0")
        self.assertEqual(res, 256)

    def test_ls_exist(self, force="", experiment="nova"):
        res = os.system("EXPERIMENT=%s ifdh ls /pnfs/nova/scratch/users 0 %s> /dev/null 2>&1" % (experiment, force))
        self.assertEqual(res,0) 

    def test_ls_exist_gridftp(self):
        self.test_ls_noexist(force="--force=gridftp")

    def test_ls_exist_srm(self):
        self.test_ls_noexist(force="--force=srm")

    def test_ls_exist_local(self):
        res = os.system("ifdh ls /tmp 0")
        self.assertEqual(res, 0)

    def test_cp_noexist_src(self, force="", experiment="nova"):
        res = os.system("EXPERIMENT=%s ifdh cp /pnfs/nova/scratch/users/nosuchuser/nosuchfile /tmp/nosuchfile %s> /dev/null 2>&1" % (experiment, force))
        self.assertEqual(res, 256)
   
    def test_cp_noexist_dst(self, force="", experiment="nova"):
        fname = "/tmp/testfile%d" % os.getpid()
        f = open(fname, "w")
        f.write("hello world\n")
        f.close()
        res = os.system("EXPERIMENT=%s ifdh cp /pnfs/nova/scratch/users/nosuchuser/nosuchfile %s %s> /dev/null 2>&1" % (experiment, fname, force))
        self.assertEqual(res, 256)
   
    def test_cp_noexist_src_gridftp(self):
        self.test_cp_noexist_dst(force="--force=gridftp")

    def test_cp_noexist_src_expgridftp(self):
        self.test_cp_noexist_dst(force="--force=expgridftp")

    def test_cp_noexist_src_srm(self):
        self.test_cp_noexist_dst(force="--force=srm")

    def test_cp_noexist_dst_gridftp(self):
        self.test_cp_noexist_dst(force="--force=gridftp")

    def test_cp_noexist_dst_expgridftp(self):
        self.test_cp_noexist_dst(force="--force=expgridftp")

    def test_cp_noexist_dst_srm(self):
        self.test_cp_noexist_dst(force="--force=srm")

    def test_cp_noexist_src_local(self,force="",experiment="nova"):
        fname = "/tmp/testfile%d" % os.getpid()
        f = open(fname, "w")
        f.write("hello world\n")
        f.close()
        res = os.system("EXPERIMENT=%s ifdh cp /tmp/nosuchdir/nosuchfile %s %s > /dev/null 2>&1" % (experiment, fname, force))
        self.assertEqual(res, 256)
   
    def test_cp_noexist_dst_local(self, force="", experiment="nova"):
        fname = "/tmp/testfile%d" % os.getpid()
        f = open(fname, "w")
        f.write("hello world\n")
        f.close()
        res = os.system("EXPERIMENT=%s ifdh cp %s /tmp/nosuchuser/nosuchfile %s > /dev/null 2>&1" % (experiment, fname, force))
        self.assertEqual(res, 256)

def suite():
    suite =  unittest.TestLoader().loadTestsFromTestCase(exitcodecases)
    return suite

if __name__ == '__main__':
    unittest.main()

