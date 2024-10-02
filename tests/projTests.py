#import unittest2 as unittest
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
    base_uri_fmt = "https://samweb.fnal.gov:8483/sam/%s/dev/api"
else:
    base_uri_fmt = "https://samweb.fnal.gov:8483/sam/%s/api"


class SAMCases(unittest.TestCase):
    counter = 0			# overall state
    test_file = "a9d1b4da-74ad-4c4f-8d72-c9e6507531b8-d.fcl"
    test_file2 = "e0d98f93-e5a3-41de-bf2d-207d57ea8b53-c.fcl"
    testdataset = "gen_cfg"  # dataset that exists there with one file in it
    experiment = "hypot"           # experiment/station/group name
    curproject = None		# project we've started
    curconsumer = None		# consumer we've started

    def setUp(self):
        self.ifdh_handle = ifdh.ifdh(base_uri_fmt % SAMCases.experiment)
        self.hostname = socket.gethostname()
          
    def tearDown(self):
        self.ifdh_handle.cleanup()
        self.ifdh_handle = None

    def log(self,msg):
        self.ifdh_handle.log(msg)
        print(msg)

    def assertEqual(self,a,b,test=None):
        try:
            super(SAMCases,self).assertEqual(a,b)
            self.log("PASS %s assertEqual(%s,%s)"%(test,a,b))
        except Exception as e:
            self.log("FAIL %s  assertEqual(%s,%s)"%(test,a,b))
            raise e

    def assertNotEqual(self,a,b,test=None):
        try:
            super(SAMCases,self).assertNotEqual(a,b)
            self.log("PASS %s assertNotEqual(%s,%s)"%(test,a,b))
        except:
            self.log("FAIL %s  assertNotEqual(%s,%s)"%(test,a,b))
            raise


    def doHypot(self):
        SAMCases.experiment = "hypot"
        SAMCases.defname = "gen_cfg"
        SAMCases.test_file = "a9d1b4da-74ad-4c4f-8d72-c9e6507531b8-d.fcl"
        SAMCases.test_file2 = "e0d98f93-e5a3-41de-bf2d-207d57ea8b53-c.fcl"

    def test_0_setexperiment(self):
        self.log(self._testMethodName)
        SAMCases.counter = SAMCases.counter + 1
        if SAMCases.counter == 1: 
           self.doHypot()
        if SAMCases.counter == 2: 
           raise RuntimeError("out of cases")
        os.environ["EXPERIMENT"] = SAMCases.experiment
        self.assertEqual(0,0,self._testMethodName)

    def test_1_locate_notfound(self):
        self.log(self._testMethodName)
        res = self.ifdh_handle.locateFile("nosuchfile")
        self.assertEqual(res,(),self._testMethodName )
        
    def test_1_locate_multi_notfound(self):
        self.log(self._testMethodName)
        try:
            res = self.ifdh_handle.locateFiles(["nosuchfile","nosuchfile2"])
            res = dict(res)
            print("got:", res)
        except RuntimeError:
            self.log("PASS %s"%self._testMethodName)
            pass
        except Exception as e:
            err="FAIL %s unexpected exception: %s"%(self._testMethodName,e)
            self.log(err)
            self.fail(err)
        else:
            if len(res.keys()) == 0:
                self.log("PASS %s"%self._testMethodName)
                 
            else:
                
                err="FAIL %s should returned empty dictionary..."%self._testMethodName
                self.log(err)
                self.fail(err)
        
    def test_2_locate_found(self):
        self.log(self._testMethodName)
        list = self.ifdh_handle.locateFile(SAMCases.test_file)
        self.assertNotEqual(len(list), 0,self._testMethodName)

    def test_2_locate_multi_found(self):
        self.log(self._testMethodName)
        res = self.ifdh_handle.locateFiles([SAMCases.test_file, SAMCases.test_file2])
        res = dict(res)
        print("got: ", res)
        self.assertNotEqual(SAMCases.test_file in res, False,self._testMethodName)
        self.assertNotEqual(SAMCases.test_file2 in res, False,self._testMethodName)
        self.assertNotEqual(len(list(res[SAMCases.test_file])), 0,self._testMethodName)
        self.assertNotEqual(len(list(res[SAMCases.test_file2])), 0,self._testMethodName)

    def test_3_describe_found(self):
        self.log(self._testMethodName)
        txt = self.ifdh_handle.describeDefinition(SAMCases.defname)
        self.assertNotEqual(txt,'',self._testMethodName)

    def test_4_startproject(self):
        self.log(self._testMethodName)
        SAMCases.curproject = "testproj%s_%d_%d" % (self.hostname, os.getpid(),time.time())
        url =  self.ifdh_handle.startProject(SAMCases.curproject,SAMCases.experiment, SAMCases.defname, os.environ.get('TEST_USER', os.environ['USER']), SAMCases.experiment)
        self.assertNotEqual(url, '',self._testMethodName)

    def test_5_startclient(self):
        self.log(self._testMethodName)
        cpurl = self.ifdh_handle.findProject(SAMCases.curproject,'')
        SAMCases.curconsumer = self.ifdh_handle.establishProcess(cpurl,"demo","1",self.hostname,os.environ.get('TEST_USER', os.environ['USER']), "","test suite job", 1)
        self.assertNotEqual(SAMCases.curconsumer, "",self._testMethodName)

    def test_5a_dumpProject(self):
        self.log(self._testMethodName)
        cpurl = self.ifdh_handle.findProject(SAMCases.curproject,'')
        res = self.ifdh_handle.dumpProject(cpurl)
        print("got: ", res)
        self.assertEqual(1,1,self._testMethodName)

    def test_6_getFetchNextFile(self):
        self.log(self._testMethodName)
        time.sleep(1)
        cpurl = self.ifdh_handle.findProject(SAMCases.curproject,'')
        uri = self.ifdh_handle.getNextFile(cpurl, SAMCases.curconsumer)
        print("got uri:" , uri)
        self.assertNotEqual(uri,"", self._testMethodName)
        path = self.ifdh_handle.fetchInput(uri)
        print("got path:" , path)
        res = os.access(path,os.R_OK)
        self.ifdh_handle.updateFileStatus(cpurl, SAMCases.curconsumer, uri, 'transferred')
        time.sleep(1)
        self.ifdh_handle.updateFileStatus(cpurl, SAMCases.curconsumer, uri, 'consumed')
        self.assertEqual(res,True,self._testMethodName)

    def test_7_getLastFile(self):
        self.log(self._testMethodName)
        cpurl = self.ifdh_handle.findProject(SAMCases.curproject,'')
        uri = self.ifdh_handle.getNextFile(cpurl, SAMCases.curconsumer)
        self.assertEqual(uri,'',self._testMethodName)
    
    def test_8_endProject(self):
        self.log(self._testMethodName)
        try:
            cpurl = self.ifdh_handle.findProject(SAMCases.curproject,'')
            self.ifdh_handle.endProject(cpurl)
            SAMCases.curproject = None
            self.log("PASS %s"%self._testMethodName)
        except Exception as e:
            errmsg="FAIL %s: %s"%(self._testMethodName,e)
            self.log(errmsg)
            self.fail(errmsg)

    def test_9_cleanup(self):
        try:
            self.log(self._testMethodName)
            cpurl = self.ifdh_handle.cleanup()
            SAMCases.curproject = None
            self.log("PASS %s"%self._testMethodName)
        except Exception as e:
            errmsg="FAIL %s: %s"%(self._testMethodName,e)
            self.log(errmsg)
            self.fail(errmsg)

    def xx_test_a_release_wrong_file(self):
        self.log(self._testMethodName)
        self.test_4_startproject()
        self.test_5_startclient()
        time.sleep(1)
        cpurl = self.ifdh_handle.findProject(SAMCases.curproject,'')
        uri = self.ifdh_handle.getNextFile(cpurl, SAMCases.curconsumer)
        try:
            self.ifdh_handle.updateFileStatus(cpurl, SAMCases.curconsumer, "wrongfile", 'consumed')
        except:
            # this should throw an exception!
            self.assertEqual(0,0,self._testMethodName)
        self.test_8_endProject()
        self.test_9_cleanup()


if __name__ == '__main__':
    unittest.main()
