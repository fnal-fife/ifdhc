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
        self.ifdh_handle = ifdh.ifdh()
        exp=os.getenv('EXPERIMENT')
        if exp:
            self.experiment=exp
        else:
            self.experiment="nova"
        filename = "file%s.txt" % os.getpid()     
        self.goodRemoteDir = "/pnfs/%s/scratch/users/test_ifdh_%s_%s" % (self.experiment, socket.gethostname(), os.getpid())
        self.goodRemoteFile = "%s/%s" % (self.goodRemoteDir, filename)
        self.badRemoteDir = "/pnfs/%s/nope/nope/nope"
        self.badRemoteFile = "%s/%s" % (self.badRemoteDir, filename)
        self.goodLocalFile = "/tmp/%s" % filename
        self.badLocalFile = "/tmp/nope/nope/nope"
        f = open(self.goodLocalFile, "w")
        f.write("hello world\n")
        f.close()
        self.forceMethods=["", "--force=gridftp","--force=srm","--force=expgridftp"]
        res = os.system("EXPERIMENT=%s ifdh mkdir %s "% (self.experiment, self.goodRemoteDir))
        res = os.system("EXPERIMENT=%s ifdh cp %s  %s "% (self.experiment, self.goodLocalFile, self.goodRemoteFile))

    def tearDown(self):
        res = os.system("EXPERIMENT=%s ifdh rm  %s "% (self.experiment, self.goodRemoteFile))
        res = os.system("EXPERIMENT=%s ifdh rmdir %s "% (self.experiment, self.goodRemoteDir))
        res = os.system("EXPERIMENT=%s ifdh rm %s "% (self.experiment, self.goodLocalFile))

## ll ##

    def test_ll_noexist(self):
        for fm in self.forceMethods:
            cmd = "EXPERIMENT=%s ifdh ll %s 0 %s >/dev/null 2>&1" %\
                (self.experiment, self.badRemoteDir,fm)
            res = os.system(cmd)
            self.assertNotEqual(res,0,note=cmd) 

    def test_ll_noexist_local(self):
        res = os.system("ifdh ll %s 0 > /dev/null 2>&1" % (self.badLocalFile))
        self.assertNotEqual(res, 0)

    def test_ll_exist(self):
        for fm in self.forceMethods:
            cmd = "EXPERIMENT=%s ifdh ll %s 0 %s> /dev/null 2>&1" %\
                (self.experiment, self.goodRemoteFile, fm)
            res = os.system(cmd)
            self.assertEqual(res,0,note=fm) 


    def test_ll_exist_local(self):
        res = os.system("ifdh ll /tmp 0 >/dev/null 2>&1")
        self.assertEqual(res, 0)

## lss ##



    def test_lss_noexist(self):
        for fm in self.forceMethods:
            cmd = "EXPERIMENT=%s ifdh lss %s 0 %s >/dev/null 2>&1" %\
                (self.experiment, self.badRemoteDir,fm)
            res = os.system(cmd)
            self.assertNotEqual(res,0,note=cmd) 

    def test_lss_noexist_local(self):
        res = os.system("ifdh lss %s 0 > /dev/null 2>&1" % (self.badLocalFile))
        self.assertNotEqual(res, 0)

    def test_lss_exist(self):
        for fm in self.forceMethods:
            cmd = "EXPERIMENT=%s ifdh lss %s 0 %s> /dev/null 2>&1" %\
                (self.experiment, self.goodRemoteFile, fm)
            res = os.system(cmd)
            self.assertEqual(res,0,note=fm) 


    def test_lss_exist_local(self):
        res = os.system("ifdh lss /tmp 0 >/dev/null 2>&1")
        self.assertEqual(res, 0)


## ls ##


    def test_ls_noexist(self):
        for fm in self.forceMethods:
            cmd = "EXPERIMENT=%s ifdh ls %s 0 %s >/dev/null 2>&1" %\
                (self.experiment, self.badRemoteDir,fm)
            res = os.system(cmd)
            self.assertNotEqual(res,0,note=cmd) 

    def test_ls_noexist_local(self):
        res = os.system("ifdh ls %s 0 > /dev/null 2>&1" % (self.badLocalFile))
        self.assertNotEqual(res, 0)

    def test_ls_exist(self):
        for fm in self.forceMethods:
            cmd = "EXPERIMENT=%s ifdh ls %s 0 %s> /dev/null 2>&1" %\
                (self.experiment, self.goodRemoteFile, fm)
            res = os.system(cmd)
            self.assertEqual(res,0,note=fm) 


    def test_ls_exist_local(self):
        res = os.system("ifdh ls /tmp 0 >/dev/null 2>&1")
        self.assertEqual(res, 0)


## cp ##

    def test_cpin_noexist_src(self, force=""):
        for fm in self.forceMethods:
            cmd = "EXPERIMENT=%s ifdh cp %s %s  %s> /dev/null 2>&1" %\
                    (self.experiment, self.badRemoteFile, self.goodLocalFile, force)
            res = os.system(cmd)
            self.assertNotEqual(res, 0, cmd)

    def test_cpin_noexist_dst(self, force=""):
        for fm in self.forceMethods:
            cmd = "EXPERIMENT=%s ifdh cp %s %s  %s> /dev/null 2>&1" %\
                    (self.experiment, self.goodRemoteFile, self.badLocalFile, force)
            res = os.system(cmd)
            self.assertNotEqual(res, 0, cmd)

    def test_cpin_exist(self, force=""):
        for fm in self.forceMethods:
            cmd = "EXPERIMENT=%s ifdh cp %s %s  %s> /dev/null 2>&1" %\
                    (self.experiment, self.goodRemoteFile, self.goodLocalFile, force)
            res = os.system(cmd)
            self.assertEqual(res, 0, cmd)

    def test_cpout_noexist_src(self, force=""):
        for fm in self.forceMethods:
            cmd = "EXPERIMENT=%s ifdh cp %s %s  %s> /dev/null 2>&1" %\
                    (self.experiment, self.badLocalFile, self.goodRemoteFile, force)
            res = os.system(cmd)
            self.assertNotEqual(res, 0, cmd)

    def test_cpout_noexist_dst(self, force=""):
        for fm in self.forceMethods:
            cmd = "EXPERIMENT=%s ifdh cp %s %s  %s> /dev/null 2>&1" %\
                    (self.experiment, self.goodLocalFile, self.badRemoteFile, force)
            res = os.system(cmd)
            self.assertNotEqual(res, 0, cmd)

    def test_cpout_exist(self, force=""):
        for fm in self.forceMethods:
            cmd = "EXPERIMENT=%s ifdh cp %s %s  %s> /dev/null 2>&1" %\
                    (self.experiment, self.goodLocalFile, self.goodRemoteFile, force)
            res = os.system(cmd)
            self.assertEqual(res, 0, cmd)
## more ##

    def test_more_noexist_local(self):
        res = os.system("EXPERIMENT=%s ifdh more %s  > /dev/null 2>&1" % 
                (self.experiment, self.badLocalFile))
        self.assertNotEqual(res,0)

    def test_more_noexist_remote(self):
        res = os.system("EXPERIMENT=%s ifdh more %s  > /dev/null 2>&1" % 
                (self.experiment, self.badRemoteFile))
        self.assertNotEqual(res,0)

    def test_more_exist_local(self):
        res = os.system("EXPERIMENT=%s ifdh more %s  > /dev/null 2>&1" % 
                (self.experiment, self.goodLocalFile))
        self.assertEqual(res,0)

    def test_more_exist_remote(self):
        res = os.system("EXPERIMENT=%s ifdh more %s  > /dev/null 2>&1" % 
                (self.experiment, self.goodRemoteFile))
        self.assertEqual(res,0)

## pin ##
    def test_pin_noexist(self):
        res = os.system("EXPERIMENT=%s ifdh pin %s  > /dev/null 2>&1" % 
                (self.experiment,self.badRemoteFile))
        self.assertNotEqual(res,0)

    def test_pin_exist(self):
        res = os.system("EXPERIMENT=%s ifdh pin %s  > /dev/null 2>&1" % 
                (self.experiment,self.goodRemoteFile))
        self.assertEqual(res,0)


def suite():
    suite =  unittest.TestLoader().loadTestsFromTestCase(exitcodecases)
    return suite

if __name__ == '__main__':
    unittest.main()


def suite():
    suite =  unittest.TestLoader().loadTestsFromTestCase(exitcodecases)
    return suite

if __name__ == '__main__':
    unittest.main()

