#!/bin/sh

export TEST_USER=$USER

rm -f /grid/data/$USER/f[12]

# for site in FNAL_nova Wisconsin SMU Nebraska UCSD 
# -g
for site in FNAL_nova 
do
    jobsub -g \
        --OS SL6 \
        -e TEST_USER \
	-l "+JobType = \"MC\"" \
 	--site $site \
	--tar_file_name=/nova/app/users/$USER/ifdhc_$site.tar \
	--input_tar_dir=$IFDHC_DIR \
	--overwrite_tar_file  \
	run_test
done
