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

class xproto_cases(unittest.TestCase):
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

    def setUp(self):
        os.environ['IFDH_CP_MAXRETRIES'] = "0"
        os.environ['EXPERIMENT'] =  xproto_cases.experiment
        os.environ['SAVE_CPN_DIR'] = os.environ.get('CPN_DIR','')
        os.environ['CPN_DIR'] = '/no/such/dir'
        self.ifdh_handle = ifdh.ifdh()
        self.hostname = socket.gethostname()
        self.local = '/tmp/test.txt'
        self.s3loc = 's3://nova-public-bucket-test/test.txt'
        self.username = os.environ.get('TEST_USER',os.environ.get('USER','nobody'))
        self.gridftploc = 'gsiftp://fndca1.fnal.gov/pnfs/fnal.gov/usr/nova/scratch/users/%s/test.txt'% self.username
        self.httploc = 'https://fndca4a.fnal.gov:2880/pnfs/fnal.gov/usr/nova/scratch/users/%s/httptest.txt'% self.username
        fh = open(self.local,'w')
        fh.write('testing testing 1 2 3')
        os.system('echo "setUp" >> /tmp/rm.out')
        for l in  [self.gridftploc, self.httploc, self.s3loc]:
            try:
               res = os.system("(ifdh rm " + l + "; echo res $?) >> /tmp/rm.out 2>&1")
            except:
               pass
        fh.close()

    def test_xproto_sg(self):
        self.log(self._testMethodName)
        self.ifdh_handle.cp([self.local,self.s3loc ])
        self.ifdh_handle.cp([self.s3loc, self.gridftploc ])
        res = self.ifdh_handle.ls(self.gridftploc,1,'')
        print "saw:" , res
        self.assertEqual(len(res),1,self._testMethodName)

    def test_xproto_gs(self):
        self.log(self._testMethodName)
        self.ifdh_handle.cp([self.local,self.gridftploc ])
        self.ifdh_handle.cp([self.gridftploc, self.s3loc ])
        res = self.ifdh_handle.ls(self.s3loc,1,'')
        print "saw:" , res
        self.assertEqual(len(res),2,self._testMethodName)

    def test_xproto_hg(self):
        self.log(self._testMethodName)
        self.ifdh_handle.cp([self.local,self.httploc ])
        self.ifdh_handle.cp([self.httploc, self.gridftploc ])
        res = self.ifdh_handle.ls(self.gridftploc,1,'')
        print "saw:" , res
        self.assertEqual(len(res),1,self._testMethodName)

    def test_xproto_gh(self):
        self.log(self._testMethodName)
        self.ifdh_handle.cp([self.local,self.gridftploc ])
        self.ifdh_handle.cp([self.gridftploc, self.httploc ])
        res = self.ifdh_handle.ls(self.httploc,1,'')
        print "saw:" , res
        self.assertEqual(len(res),1,self._testMethodName)
         
def suite():
    suite =  unittest.TestLoader().loadTestsFromTestCase(xproto_cases)
    return suite

if __name__ == '__main__':
    unittest.main()

