

#
# run 5 copies of an ifdhc cp with IFDH_STAGE_VIA set
# check output files to make sure only one runs the copyback
#
export EXPERIMENT=nova
export IFDH_STAGE_VIA="s3://ifdh-stage/test"
#export IFDH_DEBUG=1

dest=/pnfs/nova/scratch/users/$USER/multitest

ifdh ls $IFDH_STAGE_VIA  || ifdh mkdir $IFDH_STAGE_VIA

#watch ps --forest &
#watchpid=$!

cppid=""

for i in 1 2 3 4 5 
do 
  ifdh cp nucondb-client.tgz ${dest}/nucondb-client-$i.tgz > out_$i 2>&1 &   
  cppid="$cppid $!"
done

sleep 10
ifdh ls $IFDH_STAGE_VIA 5

wait $cppid

assert() {
   test "$@" || echo "Failed."
}

obtained_count=`grep -l 'Obtained lock' out_[1-5] | wc -w`
handed_off=`grep -l 'someone else'  out_[1-5] | wc -w`
echo "$obtained_count got locks"
echo "$handed_off let someone else do it"
assert $handed_off = 4
assert $obtained_count = 1


ifdh ls ${dest}

# cleanup 
for i in 1 2 3 4 5 
do
   ifdh rm $dest/nucondb-client-$i.tgz
done

ifdh ls $dest


