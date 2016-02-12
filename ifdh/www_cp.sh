#!/bin/sh

# "cp" style utility for web locations, uses curl...

curlopts="-f -L --silent"

# allow extra curl flags

while :
do
case "x$1" in
x-*)  curlopts="$curlopts $1"; shift;;
x*)   break;;
esac
done

src="$1"
dst="$2"

if [ -r "${X509_USER_PROXY:=/tmp/x509up_u`id -u`}" ]
then
    curlopts="$curlopts --cert $X509_USER_PROXY --key $X509_USER_PROXY --cacert $X509_USER_PROXY --capath ${X509_CERT_DIR:=/etc/grid-security}"
fi

if [ x$IFDH_UCONDB_UPASS != x ]
then
   curlopts="$curlopts --digest -u $IFDH_UCONDB_UPASS "
fi

#
# convert ucondb urls into real urls
#
ucondb_loc="http://dbdata0vm.fnal.gov:8201"
ucondb_convert() {
   echo $1 | 
       sed -e "s;^ucondb:/*\\(.*\\)/\\(.*\\)/\\(.*\\);$ucondb_loc/\\1_ucon_prod/app/data/\\2/\\3;"
}

dst=`ucondb_convert "$dst"`
src=`ucondb_convert "$src"`

#echo "$src;$dst"

case "$src;$dst" in 
http*//*\;/*) 
    curl $curlopts -o "$dst" "$src" 
    ;;
/*\;http*://*) 
    curl $curlopts -T "$src" "$dst"
    ;;
http*://*\;http*://*)
    curl $curlopts -o - "$src" | curl $curlopts  -T - "$dst"
    ;;
--ls*|--mv*|--rmdir*|--mkdir*|--chmod*)
    echo "Not yet implemented" >&2
    exit 1
    ;;
esac
