#!/bin/sh

export EXPERIMENT=samdev
GROUP=fermilab
proxyfile=`ifdh getProxy`

for try in 1 2 3 4 5 6 7 8 9 10
do
    #setup
    rm -f /tmp/x509up_u${UID} $proxyfile

    for i in 1 2 3 4 5
    do
        ifdh getProxy > /dev/null &
    done
    wait

    printf "Try $try: "
    voms-proxy-info -all -file $proxyfile | grep $GROUP/Role=Analysis
done
