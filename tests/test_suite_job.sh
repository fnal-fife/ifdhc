#!/bin/sh

#-----------where are we?
case "$0" in
/*) dir=`dirname $0` ;;
*)  dir=`dirname $PWD/$0`;;
esac
dir=`dirname $dir`
echo "Found myself in $dir"
#-----------

echo "proxy info:"
echo " --------------------- "
grid-proxy-info
echo " --------------------- "

# need this for our real username
export USER=`basename $CONDOR_TMP`

echo "proxy info:"
echo " --------------------- "
grid-proxy-info
echo " --------------------- "

# setup
source /grid/fermiapp/products/common/etc/setups.sh
cd $dir
setup -P -r $dir -M ups -m ifdhc.table ifdhc

cd $dir/tests &&  echo "I am in $dir/tests"

export IFDH_DEBUG=1

echo "Environment:"
echo "-------------"
printenv
echo "-------------"

set -x
exec 2>&1 

ulimit -d 1024000 -m 2048000

python projTests.py
python cpTests.py

exit 0

# this is for massively parallel tests of rest of cpTests if they get killed

case $PROCESS in
    0) python projTests.py ;;
    1) python cpTests.py  ifdh_cp_cases.test_00_fetchinput_fail ;;
    2) python cpTests.py  ifdh_cp_cases.test_0_OutputFiles ;;
    3) python cpTests.py  ifdh_cp_cases.test_0_OutputRenameFiles ;;
    4) python cpTests.py  ifdh_cp_cases.test_01_OutputRenameFiles ;;
    5) python cpTests.py  ifdh_cp_cases.test_1_OutputFiles ;;
    6) python cpTests.py  ifdh_cp_cases.test_1_OutputRenameFiles ;;
    7) python cpTests.py  ifdh_cp_cases.test_11_OutputRenameFiles ;;
    8) python cpTests.py  ifdh_cp_cases.test_1_list_copy ;;
    9) python cpTests.py  ifdh_cp_cases.test_2_list_copy_ws ;;
   10) python cpTests.py  ifdh_cp_cases.test_dirmode_expftp ;;
   11) python cpTests.py  ifdh_cp_cases.test_gsiftp__out ;;
   12) python cpTests.py  ifdh_cp_cases.test_gsiftp_in ;;
   13) python cpTests.py  ifdh_cp_cases.test_explicit_gsiftp__out ;;
   14) python cpTests.py  ifdh_cp_cases.test_explicit_gsiftp_in ;;
   15) python cpTests.py  ifdh_cp_cases.test_expftp__out ;;
   16) python cpTests.py  ifdh_cp_cases.test_expftp_in ;;
   17) python cpTests.py  ifdh_cp_cases.test_srm__out ;;
   18) python cpTests.py  ifdh_cp_cases.test_srm_in ;;
   19) python cpTests.py  ifdh_cp_cases.test_explicit_srm__out ;;
   20) python cpTests.py  ifdh_cp_cases.test_explicit_srm_in ;;
   21) python cpTests.py  ifdh_cp_cases.test_00_default__out ;;
   22) python cpTests.py  ifdh_cp_cases.test_01_default_in ;;
   23) python cpTests.py  ifdh_cp_cases.test_dd ;;
   24) python cpTests.py  ifdh_cp_cases.test_recursive ;;
   25) python cpTests.py  ifdh_cp_cases.test_recursive_globus ;;
   26) python cpTests.py  ifdh_cp_cases.test_recursive_globus_w_args ;;
   27) python cpTests.py  ifdh_cp_cases.test_dirmode ;;
   28) python cpTests.py  ifdh_cp_cases.test_dirmode_globus ;;
   29) python cpTests.py  ifdh_cp_cases.test_dirmode_globus_args_order ;;
   30) python cpTests.py  ifdh_cp_cases.test_stage_copyback ;;
   31) python cpTests.py  ifdh_cp_cases.test_borked_copyback ;;
   32) python cpTests.py  ifdh_cp_cases.test_copy_fail_dd ;;
   33) python cpTests.py  ifdh_cp_cases.test_pnfs_rewrite_1 ;;
   34) python cpTests.py  ifdh_cp_cases.test_pnfs_rewrite_2 ;;
   35) python cpTests.py  ifdh_cp_cases.test_pnfs_ls ;;
   36) python cpTests.py  ifdh_cp_cases.test_bluearc_ls_gftp ;;
   37) python cpTests.py  ifdh_cp_cases.test_pnfs_ls_gftp ;;
   38) python cpTests.py  ifdh_cp_cases.test_pnfs_ls_srm ;;
   39) python cpTests.py  ifdh_cp_cases.test_pnfs_mkdir_add ;;
   40) python cpTests.py  ifdh_cp_cases.test_expgridftp_mkdir_add ;;
   41) python cpTests.py  ifdh_cp_cases.test_bestman_mkdir_add ;;
   42) python cpTests.py  ifdh_cp_cases.test_dcache_bluearc ;;
   43) python cpTests.py  ifdh_cp_cases.test_bluearc_dcache ;;
   44) python cpTests.py  ifdh_cp_cases.test_list_copy_force_gridftp ;;
   45) python cpTests.py  ifdh_cp_cases.test_list_copy_force_dd ;;
   46) python cpTests.py  ifdh_cp_cases.test_list_copy_mixed ;;
esac
