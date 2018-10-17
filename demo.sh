#!/bin/sh

# set the base URI for our samweb service

export EXPERIMENT=uboone

# test locate/describe
#ifdh locateFile  "MV_00003142_0014_numil_v09_1105080215_RawDigits_v1_linjc.root"
#ifdh describeDefinition  "mwm_test_2"

# now start a project, and consume its files, alternately skipping or consuming
# them...
projname=mwm_`date +%Y%m%d%H`_$$
cpurl=`ifdh startProject $projname  uboone mwm_test_2 mengel uboone `
sleep 2
cpurl=`ifdh findProject  $projname uboone `
consumer_id=`ifdh establishProcess $cpurl demo 1 bel-kwinith.fnal.gov mengel "" "" "" `
flag=true
furi=`ifdh getNextFile $cpurl $consumer_id`
while [ "$furi"  != "" ]
do
	fname=`ifdh fetchInput $furi | tail -1 `
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
        rm -f $fname
        furi=`ifdh getNextFile $cpurl $consumer_id`
done
ifdh setStatus $cpurl $consumer_id  bad
ifdh endProject $cpurl 
ifdh cleanup
