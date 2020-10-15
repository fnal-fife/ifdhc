#!/bin/sh

# "cp" style utility for web locations, uses curl...

curlopts="-f -L --silent "

# allow extra curl flags

while :
do
case "x$1" in
x--ll*|x--ls*|x--mv*|x--rmdir*|x--mkdir*|x--chmod*) break;;
x-*)  curlopts="$curlopts $1"; shift;;
x*)   break;;
esac
done

src="$1"
dst="$2"

if [ -r "${X509_USER_PROXY:=/tmp/x509up_u`id -u`}" ]
then
    curlopts="$curlopts --cert $X509_USER_PROXY --key $X509_USER_PROXY --cacert $X509_USER_PROXY --capath ${X509_CERT_DIR:=/etc/grid-security/certificates}"
fi

#
# fun with scitokens...
#
if [ "x${BEARER_TOKEN}" != "x" ]
then
    curlopts="$curlopts -H "Authorization: Bearer ${BEARER_TOKEN}"
fi

if [ -r "${BEARER_TOKEN_FILE:=$XDG_RUNTIME_DIR/bt_u`id -u`}" ]
then
    curlopts="$curlopts -H "Authorization: Bearer `cat ${BEARER_TOKEN_FILE}`"
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

wmkdir() {
   curl $curlopts -o - -X MKCOL "$1"
   if [ $? = 22 ]
   then
       echo "error: File exists" >&2
   fi
}

wll() {
    curl $curlopts -o -  -H "Depth: 1" -X PROPFIND "$1" <<EOF  
<?xml version="1.0"?>
<a:propfind xmlns:a="DAV:">
 <a:allprop/>
</a:propfind>
EOF
}

wls() {
  wll "$1"  | perl -pe 's/>/>\n/go;'  |  egrep '</d:href>|</d:getcontentlength>|<d:getcontentlength/>'  | perl -pe 'if (/d:href/) { chomp();} s{<d:getcontentlength/>}{0 }; s{<[^>]*>}{ }go;'
}

dst=`ucondb_convert "$dst"`
src=`ucondb_convert "$src"`

#echo "$src;$dst"

case "$src;$dst" in 
http*//*\;/*) 
    curl $curlopts -o "$dst" "$src" 
    ;;
/*\;http*://*) 
    ( cat ) < $src | curl $curlopts -T - "$dst"
    ;;
http*://*\;http*://*)
    curl $curlopts -o - "$src" | curl $curlopts  -T - "$dst"
    ;;
--ls*) 
  
    wls "$dst"
    ;;
--ll*)

    wll "$dst"
    ;;

--mkdir*)

    wmkdir "$dst"
    ;;

--ls*|--mv*|--rmdir*|--mkdir*|--chmod*)
    echo "Not yet implemented" >&2
    exit 1
    ;;
esac
