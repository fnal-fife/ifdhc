#!/usr/bin/env python

import os
import time

if not os.environ.has_key("EXPERIMENT"):
    os.environ["EXPERIMENT"] = "samdev"

import ifdh

i = ifdh.ifdh()

# test locate/describe
print i.locateFile("MV_00003142_0014_numil_v09_1105080215_RawDigits_v1_linjc.root")
print i.describeDefinition("mwm_test_2")

# now start a project, and consume its files, alternately skipping or consuming
# them...
projname=time.strftime("mwm_%Y%m%d%H_%%d")%os.getpid()
cpurl=i.startProject(projname,"samdev", "gen_cfg", "mengel", "samdev")
time.sleep(2)
cpurl=i.findProject(projname,"samdev")
consumer_id=i.establishProcess( cpurl, "demo","1", "bel-kwinith.fnal.gov","mengel" )
print "got cpurl, consumer_id: ", cpurl, consumer_id
flag=True
furi=i.getNextFile(cpurl,consumer_id)
print "received furi: ", furi
while furi:
	fname=i.fetchInput(furi)
        if flag:
		i.updateFileStatus(cpurl, consumer_id, fname, 'transferred')
		time.sleep(1)
		i.updateFileStatus(cpurl, consumer_id, fname, 'consumed')
                flag=False
        else:
		i.updateFileStatus(cpurl, consumer_id, fname, 'skipped')
                flag=True
        try:
            os.unlink(fname)
        except:
            pass
        furi=i.getNextFile(cpurl,consumer_id)
        print "received furi: ", furi
i.setStatus( cpurl, consumer_id, "bad")
i.endProject( cpurl )
i.cleanup()
