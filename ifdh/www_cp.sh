#!/bin/sh

# "cp" style utility for web locations, uses curl...

curlopts="-L --silent"
if [ -r "${X509_USER_PROXY:=/tmp/x509up_u`id -u`}" ]
then
    curlopts="$curlopts --cert $X509_USER_PROXY --key $X509_USER_PROXY --cacert $X509_USER_PROXY --capath ${X509_CERT_DIR:=/etc/grid-security}"
fi

echo "$1;$2"
case "$1;$2" in 
http*//*\;/*) 
    curl $curlopts -o "$2" "$1"
    ;;
/*\;http*://*|/*\;https://) 
    curl $curlopts -T "$1" "$2"
    ;;
http*://*\;|http*://*)
    curl $curlopts -o - "$1" | curl $curlopts  -T - "$2"
    ;;
esac
