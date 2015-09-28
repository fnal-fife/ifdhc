

#
# run 5 copies of an ifdhc cp with IFDH_STAGE_VIA set
# check output files to make sure only one runs the copyback
#
export EXPERIMENT=nova
export IFDH_STAGE_VIA="srm://fndca1.fnal.gov:8443/srm/managerv2?SFN=/pnfs/fnal.gov/usr/nova/scratch/ifdh_stage/test_multi"
export IFDH_DEBUG=1

ifdh ls $IFDH_STAGE_VIA || ifdh mkdir $IFDH_STAGE_VIA

#watch ps --forest &
#watchpid=$!

# cleanup  past attempts...
for i in 1 2 3 4 5 
do
   srmrm srm://fg-bestman1.fnal.gov:10443/srm/v2/server?SFN=/grid/data/mengel/nucondb-client-$i.tgz > /dev/null 2>&1
done

cppid=""

for i in 1 2 3 4 5 
do 
  ifdh cp nucondb-client.tgz /grid/data/mengel/nucondb-client-$i.tgz > out_$i 2>&1 &   
  cppid="$cppid $!"
done

sleep 10
ifdh ls $IFDH_STAGE_VIA 5

wait $cppid
#kill $watchpid

assert() {
   test "$@" || echo "Failed."
}

obtained_count=`grep -l 'Obtained lock' out_[1-5] | wc -w`
handed_off=`grep -l 'someone else'  out_[1-5] | wc -w`
echo "$obtained_count got locks"
echo "$handed_off let someone else do it"
assert $handed_off = 4
assert $obtained_count = 1

srmls srm://fg-bestman1.fnal.gov:10443/srm/v2/server?SFN=/grid/data/mengel/

# cleanup 
for i in 1 2 3 4 5 
do
   srmrm srm://fg-bestman1.fnal.gov:10443/srm/v2/server?SFN=/grid/data/mengel/nucondb-client-$i.tgz
done

srmls srm://fg-bestman1.fnal.gov:10443/srm/v2/server?SFN=/grid/data/mengel/
