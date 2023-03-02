#import unittest2 as unittest
import unittest
import contextlib
import ifdh
import socket
import os
import time
import glob
import sys
from tempfile import NamedTemporaryFile
try:
   import configparser as ConfigParser
except:
   import ConfigParser

base_uri_fmt = "https://samweb.fnal.gov:8483/sam/%s/api"

# get our dcache host from the config file so we can test
# alternate dcache instances...
cp = ConfigParser.ConfigParser()
cp.read(os.environ["IFDHC_CONFIG_DIR"]+"/ifdh.cfg")
#dcache_host = cp.get('location dcache_stken', 'prefix_srm').replace("srm://","").replace("/pnfs/fnal.gov/usr/","")
dcache_host = "fndcadoor.fnal.gov"

@contextlib.contextmanager
def redir_stderr_fd():
    dnfd = os.open("/dev/null", os.O_WRONLY)
    sys.stderr.flush()
    savestderr = os.dup(2)
    os.dup2(dnfd, 2)
    try:
        yield savestderr
    finally:
        sys.stderr.flush()
        os.dup2(savestderr, 2)
        os.close(dnfd)

class Skipped(EnvironmentError):
    pass

class ifdh_cp_cases(unittest.TestCase):
    experiment = "nova"
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

    def mk_remote_dir(self,dir,opts=''):
        try:
            self.ifdh_handle.mkdir(dir)
        except:
            pass
        try:
            self.ifdh_handle.chmod(0o777, dir)
        except:
            pass

    def assertEqual(self,a,b,test=None):
        try:
            super(ifdh_cp_cases,self).assertEqual(a,b)
            self.log("PASS %s assertEqual(%s,%s)"%(test,a,b))
        except:
            self.log("FAIL %s  assertEqual(%s,%s)"%(test,a,b))
            raise


    def dirlog(self,msg):
        self.log(msg)
        self.mk_remote_dir('%s/%s'%(self.data_dir,msg))

    def li_remote_dir(self):

        if 1: return 0
        return self.ifdh_handle.ll(self.data_dir,1,'')


    def check_data_f1_f2(self, char="f"):
        print("check_data")
        print("==========")
        count = 0
        for l in self.ifdh_handle.ls(self.data_dir,1,''):
            print(l)
            if l.endswith("/%s1" % char) or l.endswith("/%s2" % char):
                count = count + 1
        print("==========")
        return count > 1

    def clean_dest(self):
        for f in ('test.txt', 'f1', 'f2'):
            full = '%s/%s' % ( self.data_dir, f )
            try:
               self.ifdh_handle.ls(full,1,'')
               self.ifdh_handle.rm(full,'')
            except:
               pass

    def make_local_test_txt(self):
        self.clean_dest();
        # clean out copy on data dir...
        ifdh_cp_cases.tc = ifdh_cp_cases.tc + 1
        out = open("%s/test.txt" % self.work, "w")
        out.write("testing testing %d \n" % ifdh_cp_cases.tc)
        out.close()
        os.system('chmod 775 %s/test.txt'%self.work)

    def make_remote_test_txt(self):
        out = NamedTemporaryFile(mode='w',delete=False)
        out.write("testing testing %d \n" % ifdh_cp_cases.tc)
        out.close()
        self.ifdh_handle.cp([out.name, self.data_dir + '/test.txt'])
        try:
            self.ifdh_handle.chmod("775", self.data_dir + '/test.txt')
        except:
            pass

        try:
            os.unlink(out.name)
        except:
            pass

    def check_writable(self,apath):
        try:
            self.ifdh_handle.chmod("775",  apath)
        except:
            pass

    def check_test_txt(self):
        fin = open("%s/test.txt" % self.work, "r")
        l = fin.read()
        fin.close
        return ifdh_cp_cases.tc == int(l[16:])


       
    def setUp(self):
        with redir_stderr_fd() as dupedstderr:
             self.setUp_impl()

    def setUp_impl(self):
        print("-----setup----")
        os.environ['IFDH_CP_MAXRETRIES'] = "0"
        os.environ['EXPERIMENT'] =  ifdh_cp_cases.experiment
        os.environ['SAVE_CPN_DIR'] = os.environ.get('CPN_DIR','')
        os.environ['CPN_DIR'] = '/no/such/dir'
        self.ifdh_handle = ifdh.ifdh(base_uri_fmt % ifdh_cp_cases.experiment)
        self.hostname = socket.gethostname()
        self.work="%s/work%d" % (os.environ.get('TMPDIR','/tmp'),os.getpid())
        self.data_dir_root="/pnfs/fnal.gov/usr/%s/scratch/users/%s/%s" % (ifdh_cp_cases.experiment, os.environ.get('TEST_USER', os.environ['USER']), self.hostname)
        self.blue_data_dir_root="/%s/data/users/%s/%s" % (ifdh_cp_cases.experiment, os.environ.get('TEST_USER', os.environ['USER']), self.hostname)
        self.data_dir="%s/%s" % (self.data_dir_root, os.getpid())
        self.blue_data_dir="%s/%s" % (self.blue_data_dir_root, os.getpid())
        for d in [self.data_dir_root, self.data_dir, '%s/started'% (self.data_dir), '/pnfs/nova/scratch/ifdh_stage/test', self.blue_data_dir]:
            try:
                # print("trying to mkdir: ", d, "... ",)
                self.ifdh_handle.mkdir(d,'')
                #print("made it.")
            except:
                print("exception.")
                pass
        self.ifdh_handle.chmod('0775', self.data_dir,'')
        self.ifdh_handle.chmod('0775', '%s/started'% (self.data_dir),'')
        # setup test directory tree..
        count = 0
        os.mkdir("%s" % (self.work))
        for sd in [ 'a', 'a/b', 'a/c' ]:
            os.mkdir("%s/%s" % (self.work,sd))
            for i in [1 , 2]:
                count = count + 1
                f = open("%s/%s/f%d" % (self.work,sd,count),"w")
                f.write("foo\n")
                f.close()
        os.system("ls -R %s" % self.work)
        time.sleep(1) # wait for NFS
        print("-----end setup----")

    def tearDown(self):
        os.system("rm -rf %s" % self.work)
        self.mk_remote_dir('%s/finished'% (self.data_dir),'')
        print("tearDown complete.")
        pass

    def test_00_fetchinput_fail(self):
        self.log(self._testMethodName)
        f = os.popen("ifdh fetchInput file:////no/such/file")
        line = f.readline()
        self.assertEqual(line,"",self._testMethodName)

    # somehow this test breaks the others later on(?)
    def test_0_OutputFiles(self):
        self.log(self._testMethodName)
        self.ifdh_handle.cleanup()
        self.ifdh_handle.addOutputFile('%s/a/f1' % self.work)
        self.ifdh_handle.addOutputFile('%s/a/f2' % self.work)
        self.ifdh_handle.copyBackOutput(self.data_dir)
        self.ifdh_handle.cleanup()
        self.assertEqual(self.check_data_f1_f2() , True, self._testMethodName)

    def test_0_OutputRenameFiles(self):
        self.log(self._testMethodName)
        self.ifdh_handle.cleanup()
        self.ifdh_handle.addOutputFile('%s/a/f1' % self.work)
        self.ifdh_handle.addOutputFile('%s/a/f2' % self.work)
        self.ifdh_handle.renameOutput('unique')
        self.ifdh_handle.copyBackOutput(self.data_dir)
        self.ifdh_handle.cleanup()

    def test_01_OutputRenameFiles(self):
        self.log(self._testMethodName)
        self.ifdh_handle.cleanup()
        self.ifdh_handle.fetchInput('%s/a/f1' % self.work)
        self.ifdh_handle.addOutputFile('%s/a/f1' % self.work)
        self.ifdh_handle.fetchInput('%s/a/f2' % self.work)
        self.ifdh_handle.addOutputFile('%s/a/f2' % self.work)
        l1 = self.ifdh_handle.ls('%s/a' % self.work,1,"")
        self.ifdh_handle.renameOutput('s/f/g/')
        l2 = self.ifdh_handle.ls('%s/a' % self.work,1,"")
        self.ifdh_handle.copyBackOutput(self.data_dir)
        self.ifdh_handle.cleanup()
        self.assertEqual("%s/a/f1" % self.work in l1 and\
                         "%s/a/g1" % self.work in l2 and\
                         "%s/a/f2" % self.work in l1 and\
                         "%s/a/g2" % self.work in l2, True, self._testMethodName)

    def test_1_OutputFiles(self):
        self.log(self._testMethodName)
        self.ifdh_handle.cleanup()
        self.ifdh_handle.addOutputFile('%s/a/f1' % self.work)
        self.ifdh_handle.addOutputFile('%s/a/f2' % self.work)
        self.ifdh_handle.copyBackOutput(self.work)
        self.ifdh_handle.cleanup()

    def test_1_OutputRenameFiles(self):
        self.log(self._testMethodName)
        self.ifdh_handle.cleanup()
        self.ifdh_handle.addOutputFile('%s/a/f1' % self.work)
        self.ifdh_handle.addOutputFile('%s/a/f2' % self.work)
        self.ifdh_handle.renameOutput('unique')
        self.ifdh_handle.copyBackOutput(self.work)
        self.ifdh_handle.cleanup()

    def test_11_OutputRenameFiles(self):
        self.log(self._testMethodName)
        self.ifdh_handle.cleanup()
        self.ifdh_handle.addOutputFile('%s/a/f1' % self.work)
        self.ifdh_handle.addOutputFile('%s/a/f2' % self.work)
        self.ifdh_handle.renameOutput('s/f/g/')
        self.ifdh_handle.copyBackOutput(self.work)
        self.ifdh_handle.cleanup()

    def test_1_li_copy(self):
        self.log(self._testMethodName)
        self.make_local_test_txt()
        f = open("%s/li" % self.work,"w")
        f.write("%s/test.txt %s/test2.txt\n%s/test2.txt %s/test3.txt\n" % (self.work, self.work, self.work, self.work))
        f.close()
        self.ifdh_handle.cp([ "-f", "%s/li" % self.work])
        os.system("mv %s/test3.txt %s/test.txt" % (self.work, self.work))
        self.assertEqual(self.check_test_txt(), True, self._testMethodName)
         
    def test_2_li_copy_ws(self):
        """make sure li files with whitespace work."""
        self.log(self._testMethodName)
        self.make_local_test_txt()
        f = open("%s/li" % self.work,"w")
        f.write("%s/test.txt   %s/test2.txt  \n%s/test2.txt\t%s/test3.txt \n" % (self.work, self.work, self.work, self.work))
        f.close()
        self.ifdh_handle.cp([ "-f", "%s/li" % self.work])
        os.system("mv %s/test3.txt %s/test.txt" % (self.work, self.work))
        self.assertEqual(self.check_test_txt(), True, self._testMethodName)
         

    def test_gsiftp__out(self):
        self.log(self._testMethodName)
        self.make_local_test_txt()
        res = self.ifdh_handle.cp([ "--force=gridftp", "%s/test.txt"%self.work, "%s/test.txt" % self.data_dir])
        # shouldn't need this one, but we seem to?
        self.check_writable( "%s/test.txt" % self.data_dir)
        self.ifdh_handle.ll(self.data_dir, 1,"")
        li = self.ifdh_handle.ls("%s/test.txt" % self.data_dir, 1,"")
        print("got li: " , li)
        self.assertEqual(len(li),1, self._testMethodName)  

    def test_gsiftp_in(self):
        self.log(self._testMethodName)
        self.make_remote_test_txt()
        self.li_remote_dir()

    def test_xrootd__out(self):
        self.log(self._testMethodName)
        self.make_local_test_txt()
        res = self.ifdh_handle.cp([ "--force=xrootd", "%s/test.txt"%self.work, "%s/test.txt" % self.data_dir])
        # shouldn't need this one, but we seem to?
        self.check_writable( "%s/test.txt" % self.data_dir)
        self.ifdh_handle.ll(self.data_dir, 1,"")
        l1 = self.ifdh_handle.ls("%s/test.txt" % self.data_dir, 1,"")
        print("got li: " , l1)
        self.assertEqual(len(l1),1, self._testMethodName)  

    def test_xrootd_in(self):
        self.log(self._testMethodName)
        self.make_remote_test_txt()
        self.li_remote_dir()
        res = self.ifdh_handle.cp([ "--force=xrootd" , "%s/test.txt" % self.data_dir, "%s/test.txt"%self.work])
        self.assertEqual(res==0 and self.check_test_txt(), True, self._testMethodName)

    def test_explicit_gsiftp__out(self):
        self.log(self._testMethodName)
        self.make_local_test_txt()
        res = self.ifdh_handle.cp([ "%s/test.txt"%self.work, "gsiftp://%s/%s/test.txt" % (dcache_host, self.data_dir)])
        self.check_writable( "%s/test.txt" % self.data_dir)
        # shouldn't need this one, but we seem to?
        #li1 = self.ifdh_handle.ls(self.data_dir,1,"")
        l1 = self.ifdh_handle.ls("%s/test.txt" % self.data_dir, 1,"")
        self.assertEqual(len(l1),1,self._testMethodName)  

    def test_explicit_gsiftp_in(self):
        self.log(self._testMethodName)
        self.make_remote_test_txt()
        self.li_remote_dir()
        res = self.ifdh_handle.cp([ "gsiftp://%s%s/test.txt" % (dcache_host,self.data_dir), "%s/test.txt"%(self.work)])
        self.assertEqual(res, 0, self._testMethodName)
        self.assertEqual(self.check_test_txt(), True, self._testMethodName)



    def test_srm__out(self):
        self.log(self._testMethodName)
        self.make_local_test_txt()
        res = self.ifdh_handle.cp([ "--force=srm", "%s/test.txt"%self.work, "%s/test.txt" % self.data_dir])
        self.check_writable( "%s/test.txt" % self.data_dir)
        # shouldn't need this one, but we seem to?
        li1 = self.ifdh_handle.ls(self.data_dir,1,"")
        l = self.ifdh_handle.ls("%s/test.txt" % self.data_dir, 1,"")
        self.assertEqual(len(l),1,self._testMethodName)  

    def test_srm_in(self):
        self.log(self._testMethodName)
        self.make_remote_test_txt()
        self.li_remote_dir()
        res = self.ifdh_handle.cp([ "--force=srm" , "%s/test.txt"%self.data_dir, "%s/test.txt"%self.work])
        self.assertEqual(res==0 and self.check_test_txt(), True, self._testMethodName)

    def test_explicit_srm__out(self):
        self.log(self._testMethodName)
        self.make_local_test_txt()
        trim_dir =  self.data_dir.replace("/pnfs","")
        dest = "srm://%s:8443/srm/managerv2?SFN=/pnfs/fnal.gov/usr%s/test.txt" % (dcache_host,trim_dir)
        self.ifdh_handle.cp([ "%s/test.txt"%self.work, dest])
        self.check_writable( "%s/test.txt" % self.data_dir)
        # shouldn't need this one, but we seem to?
        # li1 = self.ifdh_handle.ls(self.data_dir,1,"")
        time.sleep(1)
        l = self.ifdh_handle.ls( dest, 0, "")
        print("got li: " , l)
        # some utilities give the directory *and* the file, so
        # prune the first item if it's a directory
        if len(l) > 0 and l[0][-1] == '/' and len(l) > 1:
            l = l[1:]
        self.assertEqual(len(l),1, self._testMethodName) 

    def test_explicit_srm_in(self):
        self.log(self._testMethodName)
        self.li_remote_dir()
        self.make_remote_test_txt()
        trim_dir =  self.data_dir.replace("/pnfs","")
        src = "srm://%s:8443/srm/managerv2?SFN=/pnfs/fnal.gov/usr%s/test.txt" % (dcache_host, trim_dir)
        self.ifdh_handle.cp([ src, "%s/test.txt"%self.work])
        self.assertEqual(self.check_test_txt(), True, self._testMethodName)

    def test_00_default__out(self):
        self.log(self._testMethodName)
        self.make_local_test_txt()
        self.ifdh_handle.cp([ "%s/test.txt"%self.work, "%s/test.txt" % self.data_dir])
        self.check_writable( "%s/test.txt" % self.data_dir)
        # shouldn't need this one, but we seem to?
        li1 = self.ifdh_handle.ls(self.data_dir,1,"")
        l2 = self.ifdh_handle.ls("%s/test.txt" % self.data_dir, 1,"")
        print("got li: ", l2)
        self.assertEqual(len(l2),1, self._testMethodName)  # not sure how to verify if it is remote..

    def test_01_default_in(self):
        self.log(self._testMethodName)
        self.li_remote_dir()
        self.make_remote_test_txt()
        self.ifdh_handle.cp([ "%s/test.txt"%self.data_dir, "%s/test.txt"%self.work])
        res = os.stat("%s/test.txt" % self.work)
        self.assertEqual(self.check_test_txt(), True, self._testMethodName)

    def test_dd(self):
        self.log(self._testMethodName)
        os.mkdir("%s/d" % self.work)

        self.ifdh_handle.cp(["--force=dd", "-D", "%s/a/b/f3"%self.work, "%s/a/b/f4"%self.work, "%s/d"%self.work])

        #afterwards, should have 2 files in work/d
        l4 = glob.glob("%s/d/f*" % self.work)
        self.assertEqual(len(l4), 2, self._testMethodName)

    def test_recursive(self):
        self.log(self._testMethodName)
        self.ifdh_handle.cp(["-r", "%s/a"%self.work, "%s/d"%self.work])
        
        #afterwards, should have 6 files in work/d
        l4 = glob.glob("%s/d/f*" % self.work)
        l5 = glob.glob("%s/d/b/f*" % self.work)
        l6 = glob.glob("%s/d/c/f*" % self.work)
        self.assertEqual(len(l4)+len(l5)+len(l6), 6, self._testMethodName)

    def test_recursive_globus(self):

        # local to local hoses up, try copying out and back in
        # force copy outbound by experiment ftp server, so we
        # own the files and can clean them out when done.
        self.log(self._testMethodName)
        
        self.ifdh_handle.cp([ "--force=gridftp", "-r", "%s/a"%self.data_dir, "%s/d"%self.work])

 
        #afterwards, should have 6 files in work/d
        l4 = glob.glob("%s/d/f*" % self.work)
        l5 = glob.glob("%s/d/b/f*" % self.work)
        l6 = glob.glob("%s/d/c/f*" % self.work)
        self.assertEqual(len(l4)+len(l5)+len(l6), 6, self._testMethodName)

    def test_recursive_globus_w_args(self):

        # local to local hoses up, try copying out and back in
        # force copy outbound by experiment ftp server, so we
        # own the files and can clean them out when done.
        self.log(self._testMethodName)
        
        self.ifdh_handle.cp([ "-r", "--force=gridftp", "%s/a"%self.data_dir, "%s/d"%self.work])

        #afterwards, should have 6 files in work/d
        l4 = glob.glob("%s/d/f*" % self.work)
        l5 = glob.glob("%s/d/b/f*" % self.work)
        l6 = glob.glob("%s/d/c/f*" % self.work)
        self.assertEqual(len(l4)+len(l5)+len(l6), 6, self._testMethodName)
        
        
    def test_dirmode(self):
        self.log(self._testMethodName)
        l1 = glob.glob("%s/a/f*" % self.work)
        l2 = glob.glob("%s/a/b/f*" % self.work)
        l3 = glob.glob("%s/a/c/f*" % self.work)
        os.mkdir('%s/d' % self.work)
        os.mkdir('%s/d/b' % self.work)
        os.mkdir('%s/d/c' % self.work)

        l1 = (["-D"] +
               l1 + ['%s/d'%self.work , ';'] +
               l2 + ['%s/d/b'%self.work, ';'] +
               l3 + ['%s/d/c'%self.work ] )
        
        self.ifdh_handle.cp(l1)

        #afterwards, should have 6 files in work/d
        l4 = glob.glob("%s/d/f*" % self.work)
        l5 = glob.glob("%s/d/b/f*" % self.work)
        l6 = glob.glob("%s/d/c/f*" % self.work)
        self.assertEqual(len(l4)+len(l5)+len(l6), 6, self._testMethodName)

    def test_dirmode_globus(self):

        self.log(self._testMethodName)
        l1 = glob.glob("%s/a/f*" % self.work)
        l2 = glob.glob("%s/a/b/f*" % self.work)
        l3 = glob.glob("%s/a/c/f*" % self.work)
        os.mkdir('%s/d' % self.work)
        os.mkdir('%s/d/b' % self.work)
        os.mkdir('%s/d/c' % self.work)

        lr = (["--force=gridftp", "-D"] +
               l1 + ['%s/d'%self.work , ';'] +
               l2 + ['%s/d/b'%self.work, ';'] +
               l3 + ['%s/d/c'%self.work ] )

        self.ifdh_handle.cp(lr)

        #afterwards, should have 6 files in work/d
        l4 = glob.glob("%s/d/f*" % self.work)
        l5 = glob.glob("%s/d/b/f*" % self.work)
        l6 = glob.glob("%s/d/c/f*" % self.work)
        self.assertEqual(len(l4)+len(l5)+len(l6), 6, self._testMethodName)
        
    def test_dirmode_globus_args_order(self):
        self.log(self._testMethodName)
        l1 = glob.glob("%s/a/f*" % self.work)
        l2 = glob.glob("%s/a/b/f*" % self.work)
        l3 = glob.glob("%s/a/c/f*" % self.work)
        os.mkdir('%s/d' % self.work)
        os.mkdir('%s/d/b' % self.work)
        os.mkdir('%s/d/c' % self.work)

        lr = (["-D",  "--force=gridftp" ] +
               l1 + ['%s/d'%self.work , ';'] +
               l2 + ['%s/d/b'%self.work, ';'] +
               l3 + ['%s/d/c'%self.work ] )
        self.ifdh_handle.cp(lr)

        #afterwards, should have 6 files in work/d
        l4 = glob.glob("%s/d/f*" % self.work)
        l5 = glob.glob("%s/d/b/f*" % self.work)
        l6 = glob.glob("%s/d/c/f*" % self.work)
        self.assertEqual(len(l4)+len(l5)+len(l6), 6, self._testMethodName)
 
    def test_stage_copyback_srm(self):
        self.log(self._testMethodName)
        self.clean_dest()
        expsave = os.environ.get('EXPERIMENT','')
        os.environ['EXPERIMENT'] = "nova"
        os.environ['IFDH_STAGE_VIA'] = "srm://%s:8443/srm/managerv2?SFN=/pnfs/fnal.gov/usr/nova/scratch/users/$USER/ifdh_stage/test_multi" % dcache_host
        self.ifdh_handle.addOutputFile('%s/a/f1' % self.work)
        self.ifdh_handle.addOutputFile('%s/a/f2' % self.work)
        self.ifdh_handle.copyBackOutput(self.data_dir)
        self.ifdh_handle.cleanup()
        os.environ['EXPERIMENT'] = expsave
        del os.environ['IFDH_STAGE_VIA']
        self.assertEqual(self.check_data_f1_f2(), True, self._testMethodName)

    def test_stage_copyback_gsiftp(self):
        self.log(self._testMethodName)
        self.clean_dest()
        expsave = os.environ.get('EXPERIMENT','')
        os.environ['EXPERIMENT'] = "nova"
        os.environ['IFDH_STAGE_VIA'] = "gsiftp://%s/pnfs/fnal.gov/usr/nova/scratch/users/$USER/ifdh_stage/test_multi" % dcache_host
        self.ifdh_handle.addOutputFile('%s/a/f1' % self.work)
        self.ifdh_handle.addOutputFile('%s/a/f2' % self.work)
        self.ifdh_handle.copyBackOutput(self.data_dir)
        self.ifdh_handle.cleanup()
        os.environ['EXPERIMENT'] = expsave
        del os.environ['IFDH_STAGE_VIA']
        self.assertEqual(self.check_data_f1_f2(), True, self._testMethodName)

    def test_borked_copyback(self):
        self.log(self._testMethodName)
        self.clean_dest()
        expsave = os.environ.get('EXPERIMENT','')
        os.environ['EXPERIMENT'] = "nova"
        os.environ['IFDH_STAGE_VIA'] = "srm://localhost:12345/foo/bar?SFN=/baz"
        self.ifdh_handle.addOutputFile('%s/a/f1' % self.work)
        self.ifdh_handle.addOutputFile('%s/a/f2' % self.work)
        self.ifdh_handle.copyBackOutput(self.data_dir)
        self.ifdh_handle.cleanup()
        os.environ['EXPERIMENT'] = expsave
        del os.environ['IFDH_STAGE_VIA']
        self.assertEqual(self.check_data_f1_f2(), True, self._testMethodName)

    def test_copy_fail_dd(self):
        self.log(self._testMethodName)
        res = self.ifdh_handle.cp(['--force=dd', 'nosuchfile', 'nosuchfile2']);
        self.assertEqual(res,1, self._testMethodName)

    def test_pnfs_rewrite_1(self):
         self.log(self._testMethodName)
         res = self.ifdh_handle.mkdir_p('/pnfs/nova/scratch/users/$USER/ifdh_stage/test')
         res = self.ifdh_handle.cp(['-D','%s/a/f1' % self.work,'%s/a/f2' % self.work,'/pnfs/nova/scratch/users/$USER/ifdh_stage/test'])
         time.sleep(1)
         r1 = len(self.ifdh_handle.ls('/pnfs/nova/scratch/ifdh_stage/test/f1',1,''))
         r2 = len(self.ifdh_handle.ls('/pnfs/nova/scratch/ifdh_stage/test/f2',1,''))
         self.ifdh_handle.rm('/pnfs/nova/scratch/ifdh_stage/test/f1','')
         self.ifdh_handle.rm('/pnfs/nova/scratch/ifdh_stage/test/f2','')
         self.assertEqual(r1==1 and r2==1,True, self._testMethodName)

    def test_pnfs_rewrite_2(self):
         res = self.ifdh_handle.mkdir_p('/pnfs/nova/scratch/users/$USER/ifdh_stage/test')
         self.log(self._testMethodName)
         res = self.ifdh_handle.cp(['-D','%s/a/f1' % self.work,'%s/a/f2' % self.work,'/pnfs/fnal.gov/usr/nova/scratch/users/$USER/ifdh_stage/test'])
         time.sleep(1)
         self.ifdh_handle.ll('/pnfs/nova/scratch/ifdh_stage/test/',1,'')
         r1 = len(self.ifdh_handle.ls('/pnfs/nova/scratch/ifdh_stage/test/f1',1,''))
         r2 = len(self.ifdh_handle.ls('/pnfs/nova/scratch/ifdh_stage/test/f2',1,''))
         self.ifdh_handle.rm('/pnfs/nova/scratch/ifdh_stage/test/f1','')
         self.ifdh_handle.rm('/pnfs/nova/scratch/ifdh_stage/test/f2','')
         self.assertEqual(r1==1 and r2==1,True, self._testMethodName)
        
    def test_pnfs_ls(self):
         self.log(self._testMethodName)
         l1 = self.ifdh_handle.ls('/pnfs/nova/scratch', 1, "")
         self.assertEqual(len(l1) > 0, True, self._testMethodName)

    def defunct_test_bluearc_ls_gftp(self):
         self.log(self._testMethodName)
         l1 = self.ifdh_handle.ls(self.data_dir, 1, "--force=gridftp")
         self.assertEqual(len(l1) > 0, True, self._testMethodName)

    def test_pnfs_ls_gftp(self):
         self.log(self._testMethodName)
         l1 = self.ifdh_handle.ls('/pnfs/nova/scratch', 1, "--force=gridftp")
         self.assertEqual(len(l1) > 0, True, self._testMethodName)

    def test_pnfs_ls_srm(self):
         self.log(self._testMethodName)
         l1 = self.ifdh_handle.ls('/pnfs/nova/scratch', 1, "--force=srm")
         self.assertEqual(len(l1) > 0, True, self._testMethodName)

    def test_pnfs_mkdir_add(self):
         self.log(self._testMethodName)
         dir = '/pnfs/nova/scratch/users/%s/%d' % (os.environ.get('TEST_USER', os.environ['USER']), os.getpid())
         self.ifdh_handle.mkdir(dir,'')
         self.ifdh_handle.cp( ['-D','%s/a/f1' % self.work, dir])
         li = self.ifdh_handle.ls(dir + '/f1' , 1, "")
         print("list is:", li)
         self.ifdh_handle.rm(dir + '/f1','')
         self.ifdh_handle.rmdir(dir,'')
         self.assertEqual(len(li) > 0, True, self._testMethodName)

    def defunct_test_expgridftp_mkdir_add(self):
         self.log(self._testMethodName)
         dir = "%s/%d" % (self.data_dir, os.getpid())
         self.ifdh_handle.mkdir(dir, '--force=expgridftp')
         self.ifdh_handle.cp( ['--force=expgridftp', '-D','%s/a/f1' % self.work, dir])
         l1 = self.ifdh_handle.ls(dir + '/f1' , 1, "--force=expgridftp")
         print("list is:", l1)
         self.ifdh_handle.rm(dir + '/f1','--force=expgridftp')
         self.ifdh_handle.rmdir(dir,'--force=expgridftp')
         self.assertEqual(len(l1) > 0, True, self._testMethodName)

    def defunct_test_bestman_mkdir_add(self):
         self.log(self._testMethodName)
         dir = '%s/%d' % (self.data_dir, os.getpid())
         self.ifdh_handle.mkdir(dir, '--force=gridftp')
         self.ifdh_handle.cp( ['--force=gridftp', '-D','%s/a/f1' % self.work, dir])
         l1 = self.ifdh_handle.ls(dir + '/f1' , 1, "--force=gridftp")
         print("list is:", l1)
         self.ifdh_handle.rm(dir + '/f1','--force=gridftp')
         self.ifdh_handle.rmdir(dir,'--force=gridftp')
         self.assertEqual(len(l1) > 0, True, self._testMethodName)

    def test_dcache_bluearc(self):
         self.log(self._testMethodName)
         try: 
            
             if self.ifdh_handle.ls("%s/foo.txt" % self.data_dir,0,""):
                 self.ifdh_handle.rm("%s/foo.txt" % self.data_dir)
             if self.ifdh_handle.ls("/pnfs/nova/scratch/users/mengel/foo.txt",0,"--force=srm"):
                 self.ifdh_handle.rm("/pnfs/nova/scratch/users/mengel/foo.txt","--force=srm")
         except:
             pass
         f =open("%s/foo.txt" % self.work,"w") 
         f.write("foo")
         f.close()
         self.ifdh_handle.cp(["%s/foo.txt" % self.work, "/pnfs/nova/scratch/users/mengel/foo.txt"])
         res = self.ifdh_handle.cp(["/pnfs/nova/scratch/users/mengel/foo.txt", "%s/foo.txt" % self.data_dir])
         self.assertEqual(res == 0, True, self._testMethodName)

    def test_bluearc_dcache(self):
         self.log(self._testMethodName)
         try: 
             if self.ifdh_handle.ls("%s/foo.txt" % self.data_dir,0,""):
                 self.ifdh_handle.rm("%s/foo.txt" % self.data_dir,"")
         except:
             pass
         try: 
             if self.ifdh_handle.ls("/pnfs/nova/scratch/users/mengel/foo.txt",0,""):
                 self.ifdh_handle.rm("/pnfs/nova/scratch/users/mengel/foo.txt","")
         except:
             pass
         f =open("%s/foo.txt" % self.work,"w") 
         f.write("foo")
         f.close()
         self.ifdh_handle.cp(["%s/foo.txt" % self.work, "%s/foo.txt" % self.data_dir])
         res = self.ifdh_handle.cp(["%s/foo.txt" % self.data_dir, "/pnfs/nova/scratch/users/mengel/foo.txt"])
         self.assertEqual(res == 0, True, self._testMethodName)

    def test_li_copy_force_gridftp(self):
        self.log(self._testMethodName)
        self.make_local_test_txt()
        f = open("%s/li" % self.work,"w")
        f.write("%s/test.txt %s/test2.txt\n%s/test2.txt %s/test3.txt\n" % (self.work, self.work, self.work, self.work))
        f.close()
        self.ifdh_handle.cp([ "--force=gridftp", "-f", "%s/li" % self.work])
        os.system("mv %s/test3.txt %s/test.txt" % (self.work, self.work))
        self.assertEqual(self.check_test_txt(), True, self._testMethodName)

    def test_li_copy_force_dd(self):
        self.log(self._testMethodName)
        self.make_local_test_txt()
        f = open("%s/li" % self.work,"w")
        f.write("%s/test.txt %s/test2.txt\n%s/test2.txt %s/test3.txt\n" % (self.work, self.work, self.work, self.work))
        f.close()
        self.ifdh_handle.cp([ "--force=dd", "-f", "%s/li" % self.work])
        os.system("mv %s/test3.txt %s/test.txt" % (self.work, self.work))
        self.assertEqual(self.check_test_txt(), True, self._testMethodName)

    def test_li_copy_mixed(self):
        self.log(self._testMethodName)
        self.make_local_test_txt()
        f = open("%s/li" % self.work,"w")
        f.write("%s/test.txt %s/test2.txt\n/pnfs/nova/scratch/users/mengel/foo.txt %s/foo.txt\n" % (self.data_dir, self.work, self.work ))
        f.close()
        self.ifdh_handle.cp([ "-f", "%s/li" % self.work])
        self.assertEqual(self.check_test_txt(), True, self._testMethodName)

    def xx_test_lock_bits(self):  # test dropped cause we do not lock to bluearc anymore...
        
        # make some test files
        args = []
        for fn in ["t1", "t2", "t3", "t4"]:
             full = "%s/%s" % (self.work, fn)
             args.append(full)
             f = open( full, "w")
             f.write("blah blah blah")
             f.close()
        # copying them to bluearc should get and free one lock
        # ...if we enable locking again:
        os.environ['CPN_DIR'] = os.environ['SAVE_CPN_DIR']
        print("CPN_DIR is " , os.environ['CPN_DIR'])
        print("dest is", self.blue_data_dir)
        self.ifdh_handle.mkdir_p(self.blue_data_dir)
        os.system("CPN_LOCK_GROUP=gpcf ifdh cp -D %s %s >out 2>err" % ( ' '.join(args), self.blue_data_dir))
        os.system("echo '==== out ======';cat out;echo '============='")
        f = open("err","r")
        locks = 0
        frees = 0
        for line in f:
            fields = line.split(' ')
            print(line)
            print(fields)
            if fields[0] == 'LOCK':
                if fields[8] == 'lock':
                    locks = locks + 1
                if fields[8] == 'freed':
                    frees = frees + 1
        f.close()
        print("counts: locks ", locks, " frees ", frees)
        self.assertEqual( locks, 1)
        self.assertEqual( frees, 1)
         
def suite():
    suite =  unittest.TestLoader().loadTestsFromTestCase(ifdh_cp_cases)
    return suite

if __name__ == '__main__':
    unittest.main()

