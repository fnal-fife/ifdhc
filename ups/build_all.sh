hostlist="minervagpvm02 minervagpvm03 novagpvm05 novagpvm02 novagpvm06 novagpvm03 "

cat <<'EOF'
Build bits:
-----------

mkdir /tmp/$USER$$
cd /tmp/$USER$$
setup git
git clone ssh://p-ifdhc@cdcvs.fnal.gov/cvs/projects/ifdhc/ifdhc.git
. ifdhc/ups/build_node_setup.sh
PKG=ifdhc
(cd $PKG || cd ifdh-$PKG/src && make all install)
#----- test stuff -------
cd ifdhc
setup -. ifdhc
python <<XXXX
import ifdh
XXXX
cd ..
# ----------------- cut it
cd $PKG || cd ifdh-$PKG  
setup upd
VERSION=`git describe --tags --match 'v*' `
#    
upd addproduct -. $DECLAREBITS $PKG $VERSION

cd 
rm -rf /tmp/$USER$$
EOF

multixterm -xc "ssh %n" $hostlist
