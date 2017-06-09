#!/bin/bash
#

# install ifdhc_config in a product directory


usage()
{
   echo "USAGE: `basename ${0}` <product_directory> <version>"
}

# -------------------------------------------------------------------
# shared boilerplate
# -------------------------------------------------------------------

get_this_dir() 
{
    ( cd / ; /bin/pwd -P ) >/dev/null 2>&1
    if (( $? == 0 )); then
      pwd_P_arg="-P"
    fi
    reldir=`dirname ${0}`
    thisdir=`cd ${reldir} && /bin/pwd ${pwd_P_arg}`
}

enable_ups()
{
    # make sure we can use the setup alias
    if [ -z ${UPS_DIR} ]
    then
       echo "ERROR: please setup ups"
       exit 1
    fi
    source `${UPS_DIR}/bin/ups setup ${SETUP_UPS}`
}

# -------------------------------------------------------------------
# start processing
# -------------------------------------------------------------------

product_dir=${1}
pkgver=${2}

if [ -z ${product_dir} ]
then
   echo "ERROR: please specify the local product directory"
   usage
   exit 1
fi

if [ -z ${pkgver} ]
then
   echo "ERROR: please specify the ifdhc_config version"
   usage
   exit 1
fi



package=ifdhc_config
pkgdir=${product_dir}/${package}/${pkgver}
pkgups=${product_dir}/${package}/${pkgver}/ups

get_this_dir
enable_ups

if [ -d ${pkgdir} ]
then
   echo "ERROR: ${pkgdir} already exists - bailing"
   exit 2
fi
mkdir -p ${pkgdir}
if [ ! -d ${pkgdir} ]
then
   echo "ERROR: failed to create ${pkgdir}"
   exit 1
fi
mkdir -p ${pkgups}
if [ ! -d ${pkgups} ]
then
   echo "ERROR: failed to create ${pkgups}"
   exit 1
fi


cp -p ${thisdir}/../ifdh.cfg ${pkgdir}/  ||  \
      { cat 1>&2 <<EOF
ERROR: failed to copy ${thisdir}/../ifdh.cfg to ${pkgdir}
EOF
        exit 1
      }

cp -p ${thisdir}/ifdhc_config.table ${pkgups}/ || \
      { cat 1>&2 <<EOF
ERROR: failed to copy ${thisdir}/ifdhc_config.table to ${pkgups}
EOF
        exit 1
      }

ups declare -c ${package} ${pkgver} -f NULL -r ${package}/${pkgver} -m ${package}.table -z ${product_dir}

exit 0
