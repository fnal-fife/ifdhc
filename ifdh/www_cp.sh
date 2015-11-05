#!/bin/sh

# "cp" style utility for web locations, uses curl...

src="$1"
dst="$2"

curlopts="-L --silent"
if [ -r "${X509_USER_PROXY:=/tmp/x509up_u`id -u`}" ]
then
    curlopts="$curlopts --cert $X509_USER_PROXY --key $X509_USER_PROXY --cacert $X509_USER_PROXY --capath ${X509_CERT_DIR:=/etc/grid-security}"
fi



#echo "$src;$dst"

case "$src;$dst" in 
http*//*\;/*) 
    curl $curlopts -o "$dst" "$src"
    ;;
/*\;http*://*|/*\;https://) 
    curl $curlopts -T "$src" "$dst"
    ;;
http*://*\;|http*://*)
    curl $curlopts -o - "$src" | curl $curlopts  -T - "$dst"
    ;;
--ls*|--mv*|--rmdir*|--mkdir*|--chmod*)
    echo "Not yet implemented"
    exit 1
    ;;
esac
