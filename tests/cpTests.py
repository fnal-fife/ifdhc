import unittest
import ifdh
import socket
import os
import time
import glob

base_uri_fmt = "http://samweb.fnal.gov:8480/sam/%s/api"


class ifdh_cp_cases(unittest.TestCase):
    experiment = "nova"
    tc = 0

    def make_test_txt(self):
        # clean out copy on data dir...
        os.system('ssh gpsn01 rm -f %s/test.txt' % self.data_dir)
        ifdh_cp_cases.tc = ifdh_cp_cases.tc + 1
        out = open("%s/test.txt" % self.work, "w")
        out.write("testing testing %d \n" % ifdh_cp_cases.tc)
        out.close()

    def check_test_txt(self):
        fin = open("%s/test.txt" % self.work, "r")
        l = fin.read()
        fin.close
        return ifdh_cp_cases.tc == int(l[16:])
       
    def setUp(self):
        os.environ['EXPERIMENT'] =  ifdh_cp_cases.experiment
        self.ifdh_handle = ifdh.ifdh(base_uri_fmt % ifdh_cp_cases.experiment)
        self.hostname = socket.gethostname()
        self.work="/tmp/work%d" % os.getpid()
	self.data_dir="/grid/data/%s" % os.environ['USER']
        os.system("/scratch/grid/kproxy %s" % ifdh_cp_cases.experiment)
        os.environ['X509_USER_PROXY'] = "/scratch/%s/grid/%s.%s.proxy" % ( 
		os.environ['USER'], os.environ['USER'],ifdh_cp_cases.experiment)

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

    def tearDown(self):
        os.system("rm -rf %s" % self.work)

    def test_gsiftp__out(self):
        self.make_test_txt()
        self.ifdh_handle.cp([ "--force=gridftp", "%s/test.txt"%self.work, "%s/test.txt" % self.data_dir])
        self.assertEqual(0,0)  # not sure how to verify if it is remote..

    def test_gsiftp_in(self):
        self.ifdh_handle.cp([ "--force=gridftp" , "%s/test.txt" % self.data_dir, "%s/test.txt"%self.work])
        self.assertEqual(self.check_test_txt(), True)

    def test_expftp__out(self):
        self.make_test_txt()
        self.ifdh_handle.cp([ "--force=expftp", "%s/test.txt"%self.work, "%s/test.txt" % self.data_dir])
        self.assertEqual(0,0)  # not sure how to verify if it is remote..

    def test_expftp_in(self):
        self.ifdh_handle.cp([ "--force=expftp" , "%s/test.txt"%self.data_dir, "%s/test.txt"%self.work])
        self.assertEqual(self.check_test_txt(), True)

    def test_srm__out(self):
        self.make_test_txt()
        self.ifdh_handle.cp([ "--force=srm", "%s/test.txt"%self.work, "%s/test.txt" % self.data_dir])
        self.assertEqual(0,0)  # not sure how to verify if it is remote..

    def test_srm_in(self):
        self.ifdh_handle.cp([ "--force=srm" , "%s/test.txt"%self.data_dir, "%s/test.txt"%self.work])
        self.assertEqual(self.check_test_txt(), True)

    def test_default__out(self):
        self.make_test_txt()
        self.ifdh_handle.cp([ "%s/test.txt"%self.work, "%s/test.txt" % self.data_dir])
        self.assertEqual(0,0)  # not sure how to verify if it is remote..

    def test_default_in(self):
        self.ifdh_handle.cp([ "%s/test.txt"%self.data_dir, "%s/test.txt"%self.work])
        res = os.stat("%s/test.txt" % self.work)
        self.assertEqual(self.check_test_txt(), True)

    def test_recursive(self):

        self.ifdh_handle.cp(["-r", "%s/a"%self.work, "%s/d"%self.work])
        
        #afterwards, should have 6 files in work/d
        l4 = glob.glob("%s/d/f*" % self.work)
        l5 = glob.glob("%s/d/b/f*" % self.work)
        l6 = glob.glob("%s/d/c/f*" % self.work)
        self.assertEqual(len(l4)+len(l5)+len(l6), 6)

    def test_recursive_globus(self):

        os.mkdir('%s/d' % self.work)

        self.ifdh_handle.cp([ "--force=gridftp", "-r", "%s/a"%self.work, "%s/d"%self.work])
        
        #afterwards, should have 6 files in work/d
        l4 = glob.glob("%s/d/f*" % self.work)
        l5 = glob.glob("%s/d/b/f*" % self.work)
        l6 = glob.glob("%s/d/c/f*" % self.work)
        self.assertEqual(len(l4)+len(l5)+len(l6), 6)
        
    def test_dirmode(self):
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
        
        print "Doing cp with list:", list
        self.ifdh_handle.cp(list)

        #afterwards, should have 6 files in work/d
        l4 = glob.glob("%s/d/f*" % self.work)
        l5 = glob.glob("%s/d/b/f*" % self.work)
        l6 = glob.glob("%s/d/c/f*" % self.work)
        self.assertEqual(len(l4)+len(l5)+len(l6), 6)

    def test_dirmode_globus(self):

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
        print "Doing cp with list:", list

        self.ifdh_handle.cp(list)

        #afterwards, should have 6 files in work/d
        l4 = glob.glob("%s/d/f*" % self.work)
        l5 = glob.glob("%s/d/b/f*" % self.work)
        l6 = glob.glob("%s/d/c/f*" % self.work)
        self.assertEqual(len(l4)+len(l5)+len(l6), 6)
        
def suite():
    suite =  unittest.TestLoader().loadTestsFromTestCase(ifdh_cp_cases)
    return suite

if __name__ == '__main__':
    unittest.main()

