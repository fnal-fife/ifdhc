#!/bin/sh

export TEST_USER=$USER
export GROUP=nova
export EXPERIMENT=nova


rm -f /grid/data/$USER/f[12]

# for site in FNAL_nova Wisconsin SMU Nebraska UCSD 
# -g
for site in FNAL_nova Wisconsin Nebraska 
do
    jobsub -g \
        --OS SL6 \
        -e TEST_USER \
        -e EXPERIMENT \
        -e GROUP \
	-l "+JobType = \"MC\"" \
        -l "when_to_transfer_output = ON_EXIT_OR_EVICT" \
 	--site $site \
	--tar_file_name=/nova/app/users/$USER/ifdhc_$site.tar \
	--input_tar_dir=$IFDHC_DIR \
	--overwrite_tar_file  \
	run_test
done
