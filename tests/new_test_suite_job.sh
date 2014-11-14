#!/bin/bash

#-----------where are we?
case "$0" in
/*) dir=`dirname $0` ;;
*)  dir=`dirname $PWD/$0`;;
esac
dir=`dirname $dir`
echo "Found myself in $dir"
#-----------
export dir

echo "proxy info:"
echo " --------------------- "
grid-proxy-info
echo " --------------------- "

# need this for our real username
export USER=$GRID_USER

echo "proxy info:"
echo " --------------------- "
grid-proxy-info
echo " --------------------- "

# setup
#source /cvmfs/oasis.opensciencegrid.org/fermilab/products/common/etc/setups.sh || source /grid/fermiapp/products/common/etc/setups.sh
cd $dir
#setup -P -r $dir -M ups -m ifdhc.table ifdhc
source $JSB_TMP/ifdh.sh log "executing on $HOST in $dir"
setup python
export PYTHONPATH=$dir:$PYTHONPATH
export USER=$GRID_USER
export HOST=`hostname`

cd $dir/tests &&  echo "I am in $dir/tests"

export IFDH_DEBUG=1

echo "Environment:"
echo "-------------"
printenv
echo "-------------"

#set -x
#exec 2>&1 

ulimit -s 512000 -d 512000 -m 1024000
ulimit -a
#this just causes authentiction to fail
#export IFDH_GRIDFTP_EXTRA=' -rst-interval 1 -rst-timeout 1 -rst-retries 1 '


python projTests.py
R1=$?
python cpTests.py
R2=$?
#
exit $(( $R1 && $R2 ))

#grep 'def test' cpTests.py | perl -ne 's/^.*def //; s/\(.*//; printf "ifdh_cp_cases.$_  "'| sort 

 declare -a tests=( 
  ifdh_cp_cases.test_00_default__out
  ifdh_cp_cases.test_00_fetchinput_fail
  ifdh_cp_cases.test_01_default_in
  ifdh_cp_cases.test_01_OutputRenameFiles
  ifdh_cp_cases.test_0_OutputRenameFiles
  ifdh_cp_cases.test_11_OutputRenameFiles
  ifdh_cp_cases.test_1_list_copy
  ifdh_cp_cases.test_1_OutputFiles
  ifdh_cp_cases.test_1_OutputRenameFiles
  ifdh_cp_cases.test_2_list_copy_ws
  ifdh_cp_cases.test_bestman_mkdir_add
  ifdh_cp_cases.test_bluearc_dcache
  ifdh_cp_cases.test_bluearc_ls_gftp
  ifdh_cp_cases.test_borked_copyback
  ifdh_cp_cases.test_copy_fail_dd
  ifdh_cp_cases.test_dcache_bluearc
  ifdh_cp_cases.test_dd
  ifdh_cp_cases.test_dirmode
  ifdh_cp_cases.test_dirmode_expftp
  ifdh_cp_cases.test_dirmode_globus
  ifdh_cp_cases.test_dirmode_globus_args_order
  ifdh_cp_cases.test_expftp_in
  ifdh_cp_cases.test_expftp__out
  ifdh_cp_cases.test_expgridftp_mkdir_add
  ifdh_cp_cases.test_explicit_gsiftp_in
  ifdh_cp_cases.test_explicit_gsiftp__out
  ifdh_cp_cases.test_explicit_srm_in
  ifdh_cp_cases.test_explicit_srm__out
  ifdh_cp_cases.test_gsiftp_in
  ifdh_cp_cases.test_gsiftp__out
  ifdh_cp_cases.test_list_copy_force_dd
  ifdh_cp_cases.test_list_copy_force_gridftp
  ifdh_cp_cases.test_list_copy_mixed
  ifdh_cp_cases.test_pnfs_ls
  ifdh_cp_cases.test_pnfs_ls_gftp
  ifdh_cp_cases.test_pnfs_ls_srm
  ifdh_cp_cases.test_pnfs_mkdir_add
  ifdh_cp_cases.test_pnfs_rewrite_1
  ifdh_cp_cases.test_pnfs_rewrite_2
  ifdh_cp_cases.test_recursive
  ifdh_cp_cases.test_recursive_globus
  ifdh_cp_cases.test_recursive_globus_w_args
  ifdh_cp_cases.test_srm_in
  ifdh_cp_cases.test_srm__out
  ifdh_cp_cases.test_stage_copyback
)

for JOBDESC  in "${tests[@]}" ; do
    python  cpTests.py $JOBDESC 
    export RSLT=$?
    PF="FAIL"
    if [ "$RSLT" = "0" ]; then
        PF="PASS"
    fi
    SUBJECT="$PF $JOBDESC $HOST "
    ifdh log $SUBJECT
    echo $SUBJECT
    echo $SUBJECT >&2
    if [ "$MAILTO" != "0" ]; then
        mail -s "$SUBJECT" $MAILTO  < $JSB_TMP/JOBSUB_ERR_FILE
    fi
done
