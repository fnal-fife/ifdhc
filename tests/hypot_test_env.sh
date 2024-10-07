
. /cvmfs/fermilab.opensciencegrid.org/packages/common/setup-env.sh
spack load data-dispatcher@1.27.0 metacat@4.0.1

export PATH=$PWD/../bin:$PATH
export PYTHONPATH=$PWD/../lib/python:$PYTHONPATH
export IFDHC_CONFIG_DIR=$PWD/..

export IFDH_PROXY_ENABLE=0

export GROUP=hypot
export EXPERIMENT=hypot
export SAM_WEB_BASE_URL=https://samdev.fnal.gov:8483/sam/samdev/api
export IFDH_BASE_URI=https://samdev.fnal.gov:8483/sam/samdev/api
export METACAT_SERVER_URL=https://metacat.fnal.gov:9443/hypot_meta_dev/app
export DATA_DISPATCHER_URL=https://metacat.fnal.gov:9443/hypot_dd/data
export DATA_DISPATCHER_AUTH_URL=https://metacat.fnal.gov:8143/auth/hypot_dev
export METACAT_AUTH_SERVER_URL=https://metacat.fnal.gov:8143/auth/hypot_dev


