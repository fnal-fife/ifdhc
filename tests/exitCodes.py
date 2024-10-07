#import unittest2 as unittest
import unittest
import socket
import os
import time
import sys
import hypot_test_env

#
# basically make sure copying and ls-ing nonexistent directories fails
# and existing ones succeeds, plain, and in different forced protocols
#

class exitcodecases(unittest.TestCase):

    experiment = "hypot"

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
        hypot_test_env.hypot_test_env()
        import ifdh
        self.forceMethods=["", "--force=root","--force=https",]
        sys.stdout.flush()
        sys.stderr.flush()
        print("starting setUp")
        sys.stdout.flush()
        self.ifdh_handle = ifdh.ifdh()
        os.environ['IFDH_CP_MAXRETRIES'] = "0"
        self.experiment= exitcodecases.experiment
        filename = "file%s.txt" % os.getppid()     
        os.system("rm -rf /tmp/nope")

        self.goodRemoteDir = "/pnfs/%s/scratch/users/%s/test_ifdh_%s_%s" % (self.experiment, os.getenv('GRID_USER',os.getenv('USER')), socket.gethostname(), os.getppid())
        self.goodRemoteFile = "%s/%s" % (self.goodRemoteDir, filename)
        self.badRemoteDir = "/pnfs/%s/nope/nope/nope" % self.experiment
        self.badRemoteFile = "%s/%s" % (self.badRemoteDir, filename)
        self.goodLocalFile = "/tmp/%s" % filename
        self.badLocalFile = "/tmp/nope/nope/nope"
        f = open(self.goodLocalFile, "w")
        f.write("hello world\n")
        f.close()
        os.environ["EXPERIMENT"] = self.experiment
        os.environ["IFDH_GRIDFTP_EXTRA"] = "-debug"
        res = self.ifdh_handle.mkdir(self.goodRemoteDir,"")
        del os.environ["IFDH_GRIDFTP_EXTRA"]
        if (res != 0):
            print("setUp: couldn't make ", self.goodRemoteDir)
        res = self.ifdh_handle.cp([self.goodLocalFile, self.goodRemoteFile])
        if (res != 0):
            print("setUp: couldn't create ", self.goodRemoteFile)
        res = self.ifdh_handle.ls(self.goodRemoteDir,1,"")
        if len(res) == 0:
            print("setUp: couldn't create ", self.goodRemoteFile)
        print("goodRemoteDir ls gives:" , res)
          
        sys.stdout.flush()
        sys.stderr.flush()
        print("finished setUp")
        sys.stdout.flush()

    def tearDown(self):
        sys.stdout.flush()
        sys.stderr.flush()
        print("starting tearDown")
        sys.stdout.flush()
        
        res = os.system("EXPERIMENT=%s ifdh rm %s "% (self.experiment, self.goodLocalFile))
        res = os.system("EXPERIMENT=%s ifdh rm %s "% (self.experiment, self.goodRemoteFile))
        for fname in self.ifdh_handle.ls(self.goodRemoteDir,1,''):
            if (fname == self.goodRemoteDir) :
                continue
            print("cleaning up unexpected: " , fname)
            if fname[-1] == "/":
                res = os.system("EXPERIMENT=%s ifdh rmdir  %s "% (self.experiment, fname))
            else:
                res = os.system("EXPERIMENT=%s ifdh rm  %s "% (self.experiment, fname))
        res = os.system("EXPERIMENT=%s ifdh rmdir %s "% (self.experiment, self.goodRemoteDir))
        sys.stdout.flush()
        sys.stderr.flush()
        print("finished tearDown")
        sys.stdout.flush()

## ll ## uses force

    def test_ll_noexist(self):
        for force in self.forceMethods:
            cmd = "EXPERIMENT=%s ifdh ll %s 0 %s >/dev/null 2>&1" %\
                (self.experiment, self.badRemoteDir, force)
            res = os.system(cmd)
            self.assertNotEqual(res,0,note=cmd) 

    def test_ll_noexist_local(self):
        res = os.system("ifdh ll %s 0 " % (self.badLocalFile))
        self.assertNotEqual(res, 0)

    def test_ll_exist(self):
        for force in self.forceMethods:
            cmd = "EXPERIMENT=%s ifdh ll %s 0 %s " %\
                (self.experiment, self.goodRemoteFile, force)
            res = os.system(cmd)
            self.assertEqual(res,0,note=cmd) 


    def test_ll_exist_local(self):
        res = os.system("ifdh ll /tmp 0 >/dev/null 2>&1")
        self.assertEqual(res, 0)

## lss ## uses force



    def test_lss_noexist(self):
        for force in self.forceMethods:
            cmd = "EXPERIMENT=%s ifdh lss %s 0 %s >/dev/null 2>&1" %\
                (self.experiment, self.badRemoteDir, force)
            res = os.system(cmd)
            self.assertNotEqual(res,0,note=cmd) 

    def test_lss_noexist_local(self):
        res = os.system("ifdh lss %s 0 " % (self.badLocalFile))
        self.assertNotEqual(res, 0)

    def test_lss_exist(self):
        for force in self.forceMethods:
            # cmd = "EXPERIMENT=%s ifdh lss %s 0 %s " %\
            cmd = "EXPERIMENT=%s ifdh lss %s 0 %s " %\
                (self.experiment, self.goodRemoteFile, force)
            res = os.system(cmd)
            self.assertEqual(res,0,note=cmd) 

    def test_lss_exist_local(self):
        res = os.system("ifdh lss /tmp 0 >/dev/null 2>&1")
        self.assertEqual(res, 0)


## ls ## uses force


    def test_ls_noexist(self):
        for force in self.forceMethods:
            cmd = "EXPERIMENT=%s ifdh ls %s 0 %s " % (self.experiment, self.badRemoteDir, force)
            res = os.system(cmd)
            self.assertNotEqual(res,0,note=cmd) 

    def test_ls_noexist_local(self):
        res = os.system("ifdh ls %s 0 " % (self.badLocalFile))
        self.assertNotEqual(res, 0)

    def test_ls_exist(self):
        for force in self.forceMethods:
            cmd = "EXPERIMENT=%s ifdh ls %s 0 %s " % (self.experiment, self.goodRemoteFile, force)
            res = os.system(cmd)
            self.assertEqual(res,0,note=cmd) 


    def test_ls_exist_local(self):
        res = os.system("ifdh ls /tmp 0 >/dev/null 2>&1")
        self.assertEqual(res, 0)


## cp ## uses force
## also test rm of existing files ##

    def test_cpin_noexist_src(self):
        for force in self.forceMethods:
            cmd = "EXPERIMENT=%s ifdh cp %s %s  %s " % (self.experiment, force, self.badRemoteFile, self.goodLocalFile)
            res = os.system(cmd)
            self.assertNotEqual(res, 0, cmd)

    # with some tools this creates the whole badLocalFile path and breaks later tests...
    # so commenting it out
    #def test_cpin_noexist_dst(self):
    #    for force in self.forceMethods:
    #        cmd = "EXPERIMENT=%s ifdh cp %s %s  %s " % (self.experiment, force, self.goodRemoteFile, self.badLocalFile)
    #        res = os.system(cmd)
    #        self.assertNotEqual(res, 0, cmd)


    def test_cpout_noexist_src(self):
        for force in self.forceMethods:
            cmd = "EXPERIMENT=%s ifdh cp %s %s  %s " % (self.experiment, force, self.badLocalFile, self.goodRemoteFile)
            res = os.system(cmd)
            self.assertNotEqual(res, 0, cmd)
            os.system("ifdh rm %s" % self.goodRemoteFile)

    def test_cpout_noexist_dst(self):
        for force in self.forceMethods:
            cmd = "EXPERIMENT=%s ifdh cp %s %s  %s " % (self.experiment, force, self.goodLocalFile, self.badRemoteFile)
            res = os.system(cmd)
            self.assertNotEqual(res, 0, cmd)

    def test_cpin_rm_exist(self):
        tgt_file = self.goodLocalFile + "_tst"
        rmcmd = "rm -f %s " % tgt_file
        os.system(rmcmd)
        for force in self.forceMethods:
            cmd = "EXPERIMENT=%s ifdh cp %s %s  %s " % (self.experiment, force, self.goodRemoteFile, tgt_file)
            res = os.system(cmd)
            self.assertEqual(res, 0, cmd)
            os.system(rmcmd)

    def test_cpout_rm_exist(self):
        tgt_file = self.goodRemoteFile + "_tst"
        for force in self.forceMethods:
            cmd = "EXPERIMENT=%s ifdh cp %s %s  %s " % (self.experiment, force, self.goodLocalFile, tgt_file)
            #8254 cp claims to accept --force but doesnt appear to
            #cmd = "EXPERIMENT=%s ifdh cp %s %s   " % (self.experiment, self.goodLocalFile, tgt_file)
            res = os.system(cmd)
            self.assertEqual(res, 0, cmd)
            cmd = "EXPERIMENT=%s ifdh rm %s   %s " % (self.experiment,  tgt_file, force)
            res = os.system(cmd)
            self.assertEqual(res, 0, cmd)
## more ##

    def test_more_noexist_local(self):
        res = os.system("EXPERIMENT=%s ifdh more %s  " % (self.experiment, self.badLocalFile))
        self.assertNotEqual(res,0)

    def test_more_noexist_remote(self):
        res = os.system("EXPERIMENT=%s ifdh more %s " % (self.experiment, self.badRemoteFile))
        self.assertNotEqual(res,0)

    def test_more_exist_local(self):
        res = os.system("EXPERIMENT=%s ifdh more %s  " % (self.experiment, self.goodLocalFile))
        self.assertEqual(res,0)

    def test_more_exist_remote(self):
        res = os.system("EXPERIMENT=%s ifdh more %s  " % (self.experiment, self.goodRemoteFile))
        self.assertEqual(res,0)

## pin ##
#    def test_pin_noexist(self):
#        res = os.system("EXPERIMENT=%s ifdh pin %s " % 
#                (self.experiment,self.badRemoteFile))
#        self.assertNotEqual(res,0)
#
#    def test_pin_exist(self):
#        res = os.system("EXPERIMENT=%s ifdh pin %s " % 
#                (self.experiment,self.goodRemoteFile))
#        self.assertEqual(res,0)

## checksum ##

    #this test hangs indefinitely
    def test_checksum_noexist_local(self):
        print("running: ", "EXPERIMENT=%s ifdh checksum %s" % (self.experiment, self.badLocalFile))
        res = os.system("EXPERIMENT=%s ifdh checksum %s" % (self.experiment, self.badLocalFile))
        self.assertNotEqual(res,0)

    def test_checksum_noexist_remote(self):
        #this tests fails (res==0) even though running it by hand returns non zero.
        #dont understand
        cmd = "EXPERIMENT=%s ifdh checksum %s" % (self.experiment, self.badRemoteFile)
        res = os.system(cmd)
        self.assertNotEqual(res,0)

    def test_checksum_exist_local(self):
        res = os.system("EXPERIMENT=%s ifdh checksum %s" % 
                (self.experiment, self.goodLocalFile))
        self.assertEqual(res,0)

    def test_checksum_exist_remote(self):
        res = os.system("EXPERIMENT=%s ifdh checksum %s " % 
                (self.experiment, self.goodRemoteFile))
        self.assertEqual(res,0)


## mkdir rmdir ## uses force


    def test_mkdir_rmdir_exist_remote(self):
        src=self.goodRemoteDir+"/foo"
        for force in self.forceMethods:
            sys.stderr.flush()
            print("trying with %s" % force)
            sys.stdout.flush()
            cmd = "EXPERIMENT=%s ifdh mkdir %s %s  " % (
               self.experiment, src, force
            )
            res = os.system(cmd)
            self.assertEqual(res, 0, cmd)
            cmd = "EXPERIMENT=%s ifdh rmdir %s %s " % (
               self.experiment, src, force
            )
            res = os.system(cmd)
            self.assertEqual(res, 0, cmd)

## rename ## uses force

    def test_rename_noexist_src(self):
        for force in self.forceMethods:
            cmd = "EXPERIMENT=%s ifdh rename %s %s  %s" % (
              self.experiment, self.badRemoteFile, self.goodRemoteFile, force
            )
            res = os.system(cmd)
            self.assertNotEqual(res, 0, cmd)


    def test_rename_noexist_dst(self):
        for force in self.forceMethods:
            cmd = "EXPERIMENT=%s ifdh rename  %s %s  %s " % (self.experiment, self.goodRemoteFile, self.badRemoteFile, force)
            res = os.system(cmd)
            self.assertNotEqual(res, 0, cmd)


    def test_rename_exist_remote(self):
        dst=self.goodRemoteFile
        for force in self.forceMethods:
            src=dst
            dst=src+"_x"
            cmd = "EXPERIMENT=%s ifdh rename %s %s  %s " % (self.experiment,src,dst, force)
            res = os.system(cmd)
            self.assertEqual(res, 0, cmd)

    def test_rename_exist_local(self):
        dst=self.goodLocalFile
        self.log(self.goodLocalFile)
        for force in self.forceMethods:
            src=dst
            dst=src+"_x"
            cmd = "EXPERIMENT=%s ifdh rename %s %s  %s " % (self.experiment,src,dst, force)
            res = os.system(cmd)
            if force=="":
                self.assertEqual(res, 0, cmd)
            else:
                self.assertNotEqual(res, 0, cmd)
        cmd="touch %s" % self.goodLocalFile
        self.log(cmd)
        os.system(cmd)

## mv  ##



    def test_mv_noexist_src(self):
        for force in [ '' ]:
            cmd = "EXPERIMENT=%s ifdh mv %s %s  %s" % (self.experiment, self.badRemoteFile, self.goodRemoteFile, force)
            res = os.system(cmd)
            self.assertNotEqual(res, 0, cmd)


    def test_mv_noexist_dst(self):
        for force in [ '' ]:
            cmd = "EXPERIMENT=%s ifdh mv  %s %s  %s " % (self.experiment, self.goodRemoteFile, self.badRemoteFile, force)
            res = os.system(cmd)
            self.assertNotEqual(res, 0, cmd)


    def test_mv_exist_remote(self):
        dst=self.goodRemoteFile
        for force in [ '' ]:
            src=dst
            dst=src+"_x"
            cmd = "EXPERIMENT=%s ifdh mv %s %s  %s " % (self.experiment,src,dst, force)
            res = os.system(cmd)
            self.assertEqual(res, 0, cmd)

    def test_mv_exist_local(self):
        dst=self.goodLocalFile
        self.log(self.goodLocalFile)
        for force in [ '' ]:
            src=dst
            dst=src+"_x"
            cmd = "EXPERIMENT=%s ifdh mv %s %s  %s " % (self.experiment,src,dst, force)
            res = os.system(cmd)
            self.assertEqual(res, 0, cmd)
        cmd="touch %s" % self.goodLocalFile
        self.log(cmd)
        os.system(cmd)








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




