#!/bin/sh

export TEST_USER=$USER
export GROUP=nova
export EXPERIMENT=nova

TF=/nova/app/users/$USER/ifdhc_$site.tar 

(cd $IFDHC_DIR; tar cf $TF bin lib ups )

rm -f /grid/data/$USER/f[12]

# for site in FNAL_nova Wisconsin SMU Nebraska UCSD 
# -g
for site in FNAL_nova Wisconsin Nebraska 
do
    jobsub_submit \
        --group $GROUP \
        --resource-provides=usage_model=OPPORTUNISTIC,DEDICATED \
        --OS SL6 \
        -e TEST_USER \
        -e EXPERIMENT \
        -e GROUP \
 	--site $site \
	--tar_file_name=$TF \
	file://`pwd`/../run_test
done
