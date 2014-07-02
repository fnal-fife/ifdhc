#!/bin/sh

# for site in FNAL_nova Wisconsin SMU Nebraska UCSD 
for site in FNAL_nova Wisconsin SMU Nebraska UCSD
do
    jobsub -g \
        --OS SL5,SL6 \
	-l "+JobType = \"MC\"" \
 	--site $site \
	--tar_file_name=/nova/app/users/$USER/ifdhc_$site.tar \
	--input_tar_dir=$IFDHC_DIR \
	--overwrite_tar_file  \
	run_test
done
