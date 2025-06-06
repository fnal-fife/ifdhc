#!/bin/sh

# set the base URI for our samweb service

export IFDH_BASE_URI="http://samweb-minerva.fnal.gov:20004/sam/minerva/api"

# test locate/describe
ifdh locateFile  "MV_00003142_0014_numil_v09_1105080215_RawDigits_v1_linjc.root"
ifdh describeDefinition  "mwm_test_2"

# now start a project, and consume its files, alternately skipping or consuming
# them...
projname=mwm_`date +%Y%m%d%H`_$$
cpurl=`ifdh startProject $projname  minerva mwm_test_2 mengel minerva `
sleep 2
cpurl=`ifdh findProject  $projname minerva `
consumer_id=`ifdh establishProcess $cpurl demo 1 bel-kwinith.fnal.gov mengel "" "" "" `
flag=true
while furi=`ifdh getNextFile $cpurl $consumer_id`
do
        echo "status is $?"
        if [ x$furi = x ]
        then
            break
        fi
	fname=`ifdh fetchInput $furi `
        if $flag
        then
		ifdh updateFileStatus $cpurl  $consumer_id $fname transferred
		sleep 1
		ifdh updateFileStatus $cpurl  $consumer_id $fname consumed
                flag=false
        else
	  	ifdh updateFileStatus $cpurl  $consumer_id $fname skipped
                flag=true
        fi
done
ifdh setStatus $cpurl $consumer_id  bad
ifdh endProject $cpurl 
