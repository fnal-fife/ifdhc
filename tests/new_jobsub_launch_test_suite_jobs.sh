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
tar czvf $TAR_FILE *

#Legal values for site: 
# condor_status -any --format '%s\n' glidein_site | sort | uniq
#BNL Cornell FZU Fermigridosg1 Harvard MWT2 Nebraska OSC Omaha SMU TTU UCSD UChicago MWT2
#
#export SITE=SMU to send to SMU

#Legal values for SITE_CLASS as of 10/27/14 are:FERMICLOUD_PP_PRIV1,
#FERMICLOUD_PP_PRIV,FERMICLOUD_PP,FERMICLOUD8G,FERMICLOUD,OFFSITE,
#PAID_CLOUD,DEDICATED,OPPORTUNISTIC,SLOTTEST,PAID_CLOUD_TEST

ORIG_EXE=tests/new_test_suite_job.sh
if [ "$SITE" = "" ]; then
    SITE_CLASS="DEDICATED,OPPORTUNISTIC"
    TEST_SUITE=fermigrid_test.sh
    cp $ORIG_EXE $TEST_SUITE
else
    SITE_CLASS="OFFSITE"'   '"--site $SITE"' '
    TEST_SUITE=${SITE}_test.sh
    cp $ORIG_EXE $TEST_SUITE
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
   file://$TEST_SUITE

#cleanup
rm $TEST_SUITE
