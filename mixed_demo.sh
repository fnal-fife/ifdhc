#!/bin/sh

export EXPERIMENT=hypot
#
# sample data dispatcher file loop.
#

. /scratch/mengel/packages/setup-env.sh
spack load r-m-dd-config experiment=hypot

export DATA_DISPATCHER_URL=https://metacat.fnal.gov:9443/hypot_dd/data
export METACAT_SERVER_URL=https://metacat.fnal.gov:9443/hypot_meta_dev/app
export METACAT_AUTH_SERVER_URL=https://metacat.fnal.gov:8143/auth/hypot_dev
export DATA_DISPATCHER_AUTH_URL=https://metacat.fnal.gov:8143/auth/hypot_dev

set -x 

ddisp login -m token -t ${BEARER_TOKEN_FILE:-/run/user/$(id -u)/bt_u$(id -u)} $USER

projid=$(ddisp project create files from mengel:gen_cfg)
projname=$projid

cpurl=`ifdh findProject  $projname hypot`

echo "project 'url': $cpurl"

#consid=$(ddisp worker id -n)

consumer_id=`ifdh establishProcess $cpurl demo 1 bel-kwinith.fnal.gov mengel "" "" "" `

echo "got consumer_id $consumer_id"


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
ifdh cleanup
