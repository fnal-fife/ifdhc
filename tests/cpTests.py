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

class ifdh_cp_cases(unittest.TestCase):
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
            os.system('uberftp -chmod 777 "gsiftp://fg-bestman1.fnal.gov:2811%s" > /dev/null 2>&1' % (dir))
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

    def list_remote_dir(self):

        if 1: return

        f = os.popen('srmls -2 "srm://fg-bestman1.fnal.gov:10443/srm/v2/server?SFN=%s" 2>&1' % self.data_dir, "r")
        list = f.read()
        f.close()
        return list

    def check_data_f1_f2(self, char="f"):
        print "check_data"
        print "=========="
        count = 0
        for l in self.ifdh_handle.ls(self.data_dir,1,''):
            print l
            if l.endswith("/%s1" % char) or l.endswith("/%s2" % char):
                count = count + 1
        print "=========="
        return count > 1

    def clean_dest(self):
        os.system('test -d %s && rm -f %s/test.txt || srmrm -2 "srm://fg-bestman1.fnal.gov:10443/srm/v2/server?SFN=%s/test.txt" > /dev/null 2>&1' % (self.data_dir,self.data_dir,self.data_dir))
        os.system('test -d %s && rm -f %s/f1 || srmrm -2 "srm://fg-bestman1.fnal.gov:10443/srm/v2/server?SFN=%s/f1" > /dev/null 2>&1' % (self.data_dir,self.data_dir,self.data_dir))
        os.system('test -d %s && rm -f %s/f2 ||  srmrm -2 "srm://fg-bestman1.fnal.gov:10443/srm/v2/server?SFN=%s/f2" > /dev/null 2>&1' % (self.data_dir,self.data_dir,self.data_dir))

    def make_local_test_txt(self):
        self.clean_dest();
        # clean out copy on data dir...
        ifdh_cp_cases.tc = ifdh_cp_cases.tc + 1
        out = open("%s/test.txt" % self.work, "w")
        out.write("testing testing %d \n" % ifdh_cp_cases.tc)
        out.close()
        os.system('chmod 775 %s/test.txt'%self.work)

    def make_remote_test_txt(self):
        out = NamedTemporaryFile(delete=False)
        out.write("testing testing %d \n" % ifdh_cp_cases.tc)
        out.close()
        cmd="srmcp file:///%s srm://fg-bestman1.fnal.gov:10443/srm/v2/server?SFN=%s/test.txt"%(out.name,self.data_dir)
        os.system('uberftp -chmod 775 "gsiftp://fg-bestman1.fnal.gov:2811%s" > /dev/null 2>&1' % (dir))
        rslt=os.system(cmd)
        out.unlink(out.name)

    def check_writable(self,apath):
        try:
            os.chmod(apath,0775)
        except:
            try:
                os.system('uberftp -chmod 775 "gsiftp://fg-bestman1.fnal.gov:2811%s" > /dev/null 2>&1' % (apath))
            except:
                print "cant chmod 775  %s - I tried "%apath
                pass
        pass

    def check_test_txt(self):
        fin = open("%s/test.txt" % self.work, "r")
        l = fin.read()
        fin.close
        return ifdh_cp_cases.tc == int(l[16:])


       
    def setUp(self):
        os.environ['EXPERIMENT'] =  ifdh_cp_cases.experiment
        self.ifdh_handle = ifdh.ifdh(base_uri_fmt % ifdh_cp_cases.experiment)
        self.hostname = socket.gethostname()
        self.work="%s/work%d" % (os.environ.get('TMPDIR','/tmp'),os.getpid())
	self.data_dir_root="/grid/data/%s/%s" % (os.environ.get('TEST_USER', os.environ['USER']), self.hostname)
	self.data_dir="/grid/data/%s/%s/%s" % (os.environ.get('TEST_USER', os.environ['USER']), self.hostname,os.getpid())
        self.ifdh_handle.mkdir(self.data_dir_root,'')
        self.ifdh_handle.mkdir(self.data_dir,'')
        self.ifdh_handle.mkdir('%s/started'% (self.data_dir),'')
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
        print "Setup complete."

    def tearDown(self):
        os.system("rm -rf %s" % self.work)
        self.mk_remote_dir('%s/finished'% (self.data_dir),'')
        print "tearDown complete."
        pass

    def test_00_fetchinput_fail(self):
        self.log(self._testMethodName)
        f = os.popen("ifdh fetchinput file:////no/such/file")
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

    def test_1_list_copy(self):
        self.log(self._testMethodName)
        self.make_local_test_txt()
        f = open("%s/list" % self.work,"w")
        f.write("%s/test.txt %s/test2.txt\n%s/test2.txt %s/test3.txt\n" % (self.work, self.work, self.work, self.work))
        f.close()
        self.ifdh_handle.cp([ "-f", "%s/list" % self.work])
        os.system("mv %s/test3.txt %s/test.txt" % (self.work, self.work))
        self.assertEqual(self.check_test_txt(), True, self._testMethodName)
         
    def test_2_list_copy_ws(self):
        """make sure list files with whitespace work."""
        self.log(self._testMethodName)
        self.make_local_test_txt()
        f = open("%s/list" % self.work,"w")
        f.write("%s/test.txt   %s/test2.txt  \n%s/test2.txt\t%s/test3.txt \n" % (self.work, self.work, self.work, self.work))
        f.close()
        self.ifdh_handle.cp([ "-f", "%s/list" % self.work])
        os.system("mv %s/test3.txt %s/test.txt" % (self.work, self.work))
        self.assertEqual(self.check_test_txt(), True, self._testMethodName)
         
    def test_dirmode_expftp(self):
        self.log(self._testMethodName)
        self.ifdh_handle.cp(['--force=expftp', '-D', '%s/a/f1' % self.work, '%s/a/f2' % self.work, self.data_dir])
        if (os.access("%s/f1" % self.data_dir, os.R_OK)) :
            statinfo = os.stat("%s/f1" % self.data_dir)
    	    self.assertEqual(statinfo.st_mode & 040000, 0, self._testMethodName)
        else:
            self.assertEqual(True,True, self._testMethodName)

    def test_gsiftp__out(self):
        self.log(self._testMethodName)
        self.make_local_test_txt()
        res = self.ifdh_handle.cp([ "--force=gridftp", "%s/test.txt"%self.work, "%s/test.txt" % self.data_dir])
        # shouldn't need this one, but we seem to?
        self.check_writable( "%s/test.txt" % self.data_dir)
        self.ifdh_handle.ll("%s/test.txt" % self.data_dir, 1,"")
        list = self.ifdh_handle.ls("%s/test.txt" % self.data_dir, 1,"")
        print "got list: " , list
        self.assertEqual(len(list),1, self._testMethodName)  

    def test_gsiftp_in(self):
        self.log(self._testMethodName)
        self.make_remote_test_txt()
        self.list_remote_dir()
        res = self.ifdh_handle.cp([ "--force=gridftp" , "%s/test.txt" % self.data_dir, "%s/test.txt"%self.work])
        self.assertEqual(res==0 and self.check_test_txt(), True, self._testMethodName)

    def test_explicit_gsiftp__out(self):
        self.log(self._testMethodName)
        self.make_local_test_txt()
        res = self.ifdh_handle.cp([ "%s/test.txt"%self.work, "gsiftp://if-gridftp-nova.fnal.gov%s/test.txt" % self.data_dir])
        self.check_writable( "%s/test.txt" % self.data_dir)
        # shouldn't need this one, but we seem to?
        list1 = self.ifdh_handle.ls(self.data_dir,1,"")
        list = self.ifdh_handle.ls("%s/test.txt" % self.data_dir, 1,"")
        self.assertEqual(len(list),1,self._testMethodName)  

    def test_explicit_gsiftp_in(self):
        self.log(self._testMethodName)
        self.make_remote_test_txt()
        self.list_remote_dir()
        res = self.ifdh_handle.cp([ "gsiftp://if-gridftp-nova.fnal.gov%s/test.txt" % self.data_dir, "%s/test.txt"%self.work])
        self.assertEqual(res==0 and self.check_test_txt(), True, self._testMethodName)

    def test_expftp__out(self):
        self.log(self._testMethodName)
        self.make_local_test_txt()
        res = self.ifdh_handle.cp([ "--force=expftp", "%s/test.txt"%self.work, "%s/test.txt" % self.data_dir])
        self.check_writable( "%s/test.txt" % self.data_dir)
        # shouldn't need this one, but we seem to?
        list1 = self.ifdh_handle.ls(self.data_dir,1,"")
        list = self.ifdh_handle.ls("%s/test.txt" % self.data_dir, 1,"")
        self.assertEqual(len(list),1,self._testMethodName)  

    def test_expftp_in(self):
        self.log(self._testMethodName)
        #dir=self.list_remote_dir()
        data_dir = self.ifdh_handle.ls("%s/test.txt" % self.data_dir, 1,"")
        test_text='%s/test.text'%(self.data_dir)
        #self.log('%s: searching for %s in data_dir '%(self._testMethodName,test_text))
        #self.log('%s: contents of data_dir %s'%(self._testMethodName,data_dir))
        if test_text not in data_dir:
            self.make_remote_test_txt()
        res = self.ifdh_handle.cp([ "--force=expftp" , "%s/test.txt"%self.data_dir, "%s/test.txt"%self.work])
        self.assertEqual(res==0 and self.check_test_txt(), True, self._testMethodName)

    def test_srm__out(self):
        self.log(self._testMethodName)
        self.make_local_test_txt()
        res = self.ifdh_handle.cp([ "--force=srm", "%s/test.txt"%self.work, "%s/test.txt" % self.data_dir])
        self.check_writable( "%s/test.txt" % self.data_dir)
        # shouldn't need this one, but we seem to?
        list1 = self.ifdh_handle.ls(self.data_dir,1,"")
        list = self.ifdh_handle.ls("%s/test.txt" % self.data_dir, 1,"")
        self.assertEqual(len(list),1,self._testMethodName)  

    def test_srm_in(self):
        self.log(self._testMethodName)
        self.make_remote_test_txt()
        self.list_remote_dir()
        res = self.ifdh_handle.cp([ "--force=srm" , "%s/test.txt"%self.data_dir, "%s/test.txt"%self.work])
        self.assertEqual(res==0 and self.check_test_txt(), True, self._testMethodName)

    def test_explicit_srm__out(self):
        self.log(self._testMethodName)
        self.make_local_test_txt()
        dest = "srm://fg-bestman1.fnal.gov:10443/srm/v2/server?SFN=%s/test.txt" % self.data_dir
        self.ifdh_handle.cp([ "%s/test.txt"%self.work, dest])
        self.check_writable( "%s/test.txt" % self.data_dir)
        # shouldn't need this one, but we seem to?
        list1 = self.ifdh_handle.ls(self.data_dir,1,"")
        list = self.ifdh_handle.ls( dest, 0, "")
        self.assertEqual(len(list),1, self._testMethodName) 

    def test_explicit_srm_in(self):
        self.log(self._testMethodName)
        self.list_remote_dir()
        self.make_remote_test_txt()
        self.ifdh_handle.cp([ "srm://fg-bestman1.fnal.gov:10443/srm/v2/server?SFN=%s/test.txt"%self.data_dir, "%s/test.txt"%self.work])
        self.assertEqual(self.check_test_txt(), True, self._testMethodName)

    def test_00_default__out(self):
        self.log(self._testMethodName)
        self.make_local_test_txt()
        self.ifdh_handle.cp([ "%s/test.txt"%self.work, "%s/test.txt" % self.data_dir])
        self.check_writable( "%s/test.txt" % self.data_dir)
        # shouldn't need this one, but we seem to?
        list1 = self.ifdh_handle.ls(self.data_dir,1,"")
        list = self.ifdh_handle.ls("%s/test.txt" % self.data_dir, 1,"")
        self.assertEqual(len(list),1, self._testMethodName)  # not sure how to verify if it is remote..

    def test_01_default_in(self):
        self.log(self._testMethodName)
        self.list_remote_dir()
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
        self.ifdh_handle.cp([ "--force=expftp", "-r", "%s/a"%self.work, "%s/a"%self.data_dir])
        
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
        self.ifdh_handle.cp([ "-r", "--force=expftp", "%s/a"%self.work, "%s/a"%self.data_dir])
        
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

        list = (["-D"] +
               l1 + ['%s/d'%self.work , ';'] +
               l2 + ['%s/d/b'%self.work, ';'] +
               l3 + ['%s/d/c'%self.work ] )
        
        self.ifdh_handle.cp(list)

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

        list = (["--force=gridftp", "-D"] +
               l1 + ['%s/d'%self.work , ';'] +
               l2 + ['%s/d/b'%self.work, ';'] +
               l3 + ['%s/d/c'%self.work ] )

        self.ifdh_handle.cp(list)

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

        list = (["-D",  "--force=gridftp" ] +
               l1 + ['%s/d'%self.work , ';'] +
               l2 + ['%s/d/b'%self.work, ';'] +
               l3 + ['%s/d/c'%self.work ] )
        self.ifdh_handle.cp(list)

        #afterwards, should have 6 files in work/d
        l4 = glob.glob("%s/d/f*" % self.work)
        l5 = glob.glob("%s/d/b/f*" % self.work)
        l6 = glob.glob("%s/d/c/f*" % self.work)
        self.assertEqual(len(l4)+len(l5)+len(l6), 6, self._testMethodName)
 
    def test_stage_copyback(self):
        self.log(self._testMethodName)
        self.clean_dest()
        expsave = os.environ.get('EXPERIMENT','')
        os.environ['EXPERIMENT'] = "nova"
        os.environ['IFDH_STAGE_VIA'] = "srm://fndca1.fnal.gov:8443/srm/managerv2?SFN=/pnfs/fnal.gov/usr/nova/ifdh_stage/test_multi"
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
         res = self.ifdh_handle.cp(['-D','%s/a/f1' % self.work,'%s/a/f2' % self.work,'/pnfs/nova/ifdh_stage/test'])
         r1 = os.system("srmls -2 srm://fndca1.fnal.gov:8443/pnfs/fnal.gov/usr/nova/ifdh_stage/test/f1")
         r2 = os.system("srmls -2 srm://fndca1.fnal.gov:8443/pnfs/fnal.gov/usr/nova/ifdh_stage/test/f2")
         os.system("srmrm -2 srm://fndca1.fnal.gov:8443/pnfs/fnal.gov/usr/nova/ifdh_stage/test/f1")
         os.system("srmrm -2 srm://fndca1.fnal.gov:8443/pnfs/fnal.gov/usr/nova/ifdh_stage/test/f2")
         self.assertEqual(r1==0 and r2==0,True, self._testMethodName)

    def test_pnfs_rewrite_2(self):
         self.log(self._testMethodName)
         res = self.ifdh_handle.cp(['-D','%s/a/f1' % self.work,'%s/a/f2' % self.work,'/pnfs/fnal.gov/usr/nova/ifdh_stage/test'])
         r1 = os.system("srmls -2 srm://fndca1.fnal.gov:8443/pnfs/fnal.gov/usr/nova/ifdh_stage/test/f1")
         r2 = os.system("srmls -2 srm://fndca1.fnal.gov:8443/pnfs/fnal.gov/usr/nova/ifdh_stage/test/f2")
         os.system("srmrm -2 srm://fndca1.fnal.gov:8443/pnfs/fnal.gov/usr/nova/ifdh_stage/test/f1")
         os.system("srmrm -2 srm://fndca1.fnal.gov:8443/pnfs/fnal.gov/usr/nova/ifdh_stage/test/f2")
         self.assertEqual(r1==0 and r2==0,True, self._testMethodName)
        
    def test_pnfs_ls(self):
         self.log(self._testMethodName)
         list = self.ifdh_handle.ls('/pnfs/nova/scratch', 1, "")
         self.assertEqual(len(list) > 0, True, self._testMethodName)

    def test_bluearc_ls_gftp(self):
         self.log(self._testMethodName)
         list = self.ifdh_handle.ls(self.data_dir, 1, "--force=gridftp")
         self.assertEqual(len(list) > 0, True, self._testMethodName)

    def test_pnfs_ls_gftp(self):
         self.log(self._testMethodName)
         list = self.ifdh_handle.ls('/pnfs/nova/scratch', 1, "--force=gridftp")
         self.assertEqual(len(list) > 0, True, self._testMethodName)

    def test_pnfs_ls_srm(self):
         self.log(self._testMethodName)
         list = self.ifdh_handle.ls('/pnfs/nova/scratch', 1, "--force=srm")
         self.assertEqual(len(list) > 0, True, self._testMethodName)

    def test_pnfs_mkdir_add(self):
         self.log(self._testMethodName)
         dir = '/pnfs/nova/scratch/users/%s/%d' % (os.environ.get('TEST_USER', os.environ['USER']), os.getpid())
         self.ifdh_handle.mkdir(dir,'')
         self.ifdh_handle.cp( ['-D','%s/a/f1' % self.work, dir])
         list = self.ifdh_handle.ls(dir + '/f1' , 1, "")
         print "list is:", list
         self.ifdh_handle.rm(dir + '/f1','')
         self.ifdh_handle.rmdir(dir,'')
         self.assertEqual(len(list) > 0, True, self._testMethodName)

    def test_expgridftp_mkdir_add(self):
         self.log(self._testMethodName)
         dir = "%s/%d" % (self.data_dir, os.getpid())
         self.ifdh_handle.mkdir(dir, '--force=expgridftp')
         self.ifdh_handle.cp( ['--force=expgridftp', '-D','%s/a/f1' % self.work, dir])
         list = self.ifdh_handle.ls(dir + '/f1' , 1, "--force=expgridftp")
         print "list is:", list
         self.ifdh_handle.rm(dir + '/f1','--force=expgridftp')
         self.ifdh_handle.rmdir(dir,'--force=expgridftp')
         self.assertEqual(len(list) > 0, True, self._testMethodName)

    def test_bestman_mkdir_add(self):
         self.log(self._testMethodName)
         dir = '%s/%d' % (self.data_dir, os.getpid())
         self.ifdh_handle.mkdir(dir, '--force=gridftp')
         self.ifdh_handle.cp( ['--force=gridftp', '-D','%s/a/f1' % self.work, dir])
         list = self.ifdh_handle.ls(dir + '/f1' , 1, "--force=gridftp")
         print "list is:", list
         self.ifdh_handle.rm(dir + '/f1','--force=gridftp')
         self.ifdh_handle.rmdir(dir,'--force=gridftp')
         self.assertEqual(len(list) > 0, True, self._testMethodName)

    def test_dcache_bluearc(self):
         self.log(self._testMethodName)
         try: 
             self.ifdh_handle.rm("%s/foo.txt" % self.data_dir,"")
             self.ifdh_handle.rm("/pnfs/nova/scratch/users/mengel/foo.txt","")
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
             self.ifdh_handle.rm("%s/foo.txt" % self.data_dir,"")
         except:
             pass
         try: 
             self.ifdh_handle.rm("/pnfs/nova/scratch/users/mengel/foo.txt","")
         except:
             pass
         f =open("%s/foo.txt" % self.work,"w") 
 	 f.write("foo")
	 f.close()
         self.ifdh_handle.cp(["%s/foo.txt" % self.work, "%s/foo.txt" % self.data_dir])
         res = self.ifdh_handle.cp(["%s/foo.txt" % self.data_dir, "/pnfs/nova/scratch/users/mengel/foo.txt"])
         self.assertEqual(res == 0, True, self._testMethodName)

    def test_list_copy_force_gridftp(self):
        self.log(self._testMethodName)
        self.make_local_test_txt()
        f = open("%s/list" % self.work,"w")
        f.write("%s/test.txt %s/test2.txt\n%s/test2.txt %s/test3.txt\n" % (self.work, self.work, self.work, self.work))
        f.close()
        self.ifdh_handle.cp([ "--force=gridftp", "-f", "%s/list" % self.work])
        os.system("mv %s/test3.txt %s/test.txt" % (self.work, self.work))
        self.assertEqual(self.check_test_txt(), True, self._testMethodName)

    def test_list_copy_force_dd(self):
        self.log(self._testMethodName)
        self.make_local_test_txt()
        f = open("%s/list" % self.work,"w")
        f.write("%s/test.txt %s/test2.txt\n%s/test2.txt %s/test3.txt\n" % (self.work, self.work, self.work, self.work))
        f.close()
        self.ifdh_handle.cp([ "--force=dd", "-f", "%s/list" % self.work])
        os.system("mv %s/test3.txt %s/test.txt" % (self.work, self.work))
        self.assertEqual(self.check_test_txt(), True, self._testMethodName)

    def test_list_copy_mixed(self):
        self.log(self._testMethodName)
        self.make_local_test_txt()
        f = open("%s/list" % self.work,"w")
        f.write("%s/test.txt %s/test2.txt\n/pnfs/nova/scratch/users/mengel/foo.txt %s/foo.txt\n" % (self.data_dir, self.work, self.work ))
        f.close()
        self.ifdh_handle.cp([ "-f", "%s/list" % self.work])
        self.assertEqual(self.check_test_txt(), True, self._testMethodName)
         
def suite():
    suite =  unittest.TestLoader().loadTestsFromTestCase(ifdh_cp_cases)
    return suite

if __name__ == '__main__':
    unittest.main()

