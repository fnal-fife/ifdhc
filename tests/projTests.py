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


class SAMCases(unittest.TestCase):
    counter = 0			# overall state
    testfile = None		# filename that exists there
    testdataset = "mwm_test_9"  # dataset that exists there with one file in it
    experiment = None           # experiment/station/group name
    curproject = None		# project we've started
    curconsumer = None		# consumer we've started

    def setUp(self):
        self.ifdh_handle = ifdh.ifdh(base_uri_fmt % SAMCases.experiment)
        self.hostname = socket.gethostname()
          
    def tearDown(self):
        self.ifdh_handle.cleanup()
        self.ifdh_handle = None

    def doMinerva(self):
        SAMCases.experiment = "minerva"
        if do_dev_sam:
            SAMCases.defname = "run_798_raw_2"
        else:
            SAMCases.defname = "mwm_test_9" 
        SAMCases.test_file = "MN_00000798_0002_numib_v04_0911090042_RawEvents.root"


    def doNova(self):
        SAMCases.experiment = "nova"
        SAMCases.defname = "mwm_test_9"
        SAMCases.test_file = "sim_genie_fd_nhc_fluxswap_10000_r1_38_S12.02.14_20120318_005449_reco.root"

    def test_0_setexperiment(self):
        SAMCases.counter = SAMCases.counter + 1
        if SAMCases.counter == 1: 
           self.doMinerva()
        if SAMCases.counter == 2: 
           self.doNova()
        if SAMCases.counter == 3: 
           raise RuntimeError("out of cases")
        os.environ["EXPERIMENT"] = SAMCases.experiment
        self.assertEqual(0,0)

    def test_1_locate_notfound(self):
        self.assertRaises(RuntimeError, self.ifdh_handle.locateFile,"nosuchfile")

    def test_2_locate_found(self):
        list = self.ifdh_handle.locateFile(SAMCases.test_file)
        self.assertNotEqual(len(list), 0)

    def test_3_describe_found(self):
        txt = self.ifdh_handle.describeDefinition(SAMCases.defname)
        self.assertNotEqual(txt,'')

    def test_4_startproject(self):
        SAMCases.curproject = "testproj%s_%d_%d" % (self.hostname, os.getpid(),time.time())
        url =  self.ifdh_handle.startProject(SAMCases.curproject,SAMCases.experiment, SAMCases.defname, os.environ['USER'], SAMCases.experiment)
        self.assertNotEqual(url, '')

    def test_5_startclient(self):
        cpurl = self.ifdh_handle.findProject(SAMCases.curproject,'')
        SAMCases.curconsumer = self.ifdh_handle.establishProcess(cpurl,"demo","1",self.hostname,os.environ['USER'], "","test suite job", 0)
        self.assertNotEqual(SAMCases.curconsumer, "")

    def test_5a_dumpProject(self):
        cpurl = self.ifdh_handle.findProject(SAMCases.curproject,'')
        res = self.ifdh_handle.dumpProject(cpurl)
        print "got: ", res
        self.assertEqual(1,1)

    def test_6_getFetchNextFile(self):
        time.sleep(1)
        cpurl = self.ifdh_handle.findProject(SAMCases.curproject,'')
        uri = self.ifdh_handle.getNextFile(cpurl, SAMCases.curconsumer)
        try:
           path = self.ifdh_handle.fetchInput(uri)
           res = os.access(path,os.R_OK)
        except:
           print "Exception in fetchInput:", sys.exc_info()[0]
           res = True
        self.ifdh_handle.updateFileStatus(cpurl, SAMCases.curconsumer, uri, 'transferred')
        time.sleep(1)
        self.ifdh_handle.updateFileStatus(cpurl, SAMCases.curconsumer, uri, 'consumed')
        self.assertEqual(res,True)

    def test_7_getLastFile(self):
        cpurl = self.ifdh_handle.findProject(SAMCases.curproject,'')
        uri = self.ifdh_handle.getNextFile(cpurl, SAMCases.curconsumer)
        self.assertEqual(uri,'')
    
    def test_8_endProject(self):
        cpurl = self.ifdh_handle.findProject(SAMCases.curproject,'')
        self.ifdh_handle.endProject(cpurl)
        SAMCases.curproject = None

    def test_9_cleanup(self):
        cpurl = self.ifdh_handle.cleanup()
        SAMCases.curproject = None


if __name__ == '__main__':
    unittest.main()
