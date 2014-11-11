#!/bin/sh

if [ "$GROUP" = "" ]; then
    export GROUP=nova
fi

#if JOBSUB_SERVER not set, jobs go to fifebatch.fnal.gov (production)
#export JOBSUB_SERVER="--jobsub-server=https://fifebatch-dev.fnal.gov:8443 "
#export JOBSUB_SERVER="--jobsub-server=https://fifebatch-preprod.fnal.gov:8443 "

#if MAILTO set email status reports for individual jobs get emailed back 
#export MAILTO=$USER@fnal.gov

if [ "$MAILTO" != "" ]; then
    SHOULD_SEND_MAIL=" -e MAILTO "
fi

TAR_FILE=ifdh_test.tgz
if [ ! -e "$TAR_FILE" ] ; then
    tar czvf $TAR_FILE *
fi

#Legal values for site: 
# condor_status -any --format '%s\n' glidein_site | sort | uniq
#export SITE=SMU to send to SMU

#Legal values for SITE_CLASS as of 10/27/14 are:FERMICLOUD_PP_PRIV1,
#FERMICLOUD_PP_PRIV,FERMICLOUD_PP,FERMICLOUD8G,FERMICLOUD,OFFSITE,
#PAID_CLOUD,DEDICATED,OPPORTUNISTIC,SLOTTEST,PAID_CLOUD_TEST
if [ "$SITE" = "" ]; then
    SITE_CLASS="DEDICATED,OPPORTUNISTIC"
else
    SITE_CLASS="OFFSITE"'   '"--site $SITE"' '
fi

jobsub_submit -G $GROUP  \
   $NUMJOBS \
   --disk 1 \
   --OS SL5,SL6 \
   --debug \
   --mail_never --tar_file_name dropbox://$TAR_FILE \
   --resource-provides=usage_model=$SITE_CLASS \
   $JOBSUB_SERVER \
   $SHOULD_SEND_MAIL \
   file://new_test_suite_job.sh
