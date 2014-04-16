hostlist="minervagpvm02 novagpvm02 novagpvm03 fermicloud050 fermicloud323 "

cat <<'EOF'
Build bits:
-----------

mkdir /tmp/$USER$$
cd /tmp/$USER$$
setup git
git clone ssh://p-ifdhc@cdcvs.fnal.gov/cvs/projects/ifdhc-libwda
git clone ssh://p-ifdhc@cdcvs.fnal.gov/cvs/projects/ifdhc/ifdhc.git
git clone ssh://p-ifdhc@cdcvs.fnal.gov/cvs/projects/ifdhc-ifbeam
git clone ssh://p-ifdhc@cdcvs.fnal.gov/cvs/projects/ifdhc-nucondb
. ifdhc/ups/build_node_setup.sh
(cd ifdhc && make all install)
(cd ifdhc-libwda/src && make all install)
(cd ifdhc-ifbeam/src && make all install)
(cd ifdhc-nucondb/src && make all install)
setup -. ifdhc
python <<XXXX
import ifdh
XXXX
setup upd
VERSION=`git describe --tags --match 'v*' `
make distrib
#    
upd addproduct -T ifdhc.tar.gz  -M ups -m ifdhc.table $DECLAREBITS ifdhc $VERSION
upd addproduct -T ifbeam.tar.gz -M ups -m ifbeam.table $DECLAREBITS ifbeam $VERSION
cd 
rm -rf /tmp/$USER$$
EOF

multixterm -xc "ssh %n" $hostlist
