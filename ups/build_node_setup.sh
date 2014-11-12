
#
# file to be sourced when using multixterm;
#   we use minervagpvm02 to build against minerva's python
#   we use novagpvm03 to build the 32-bit SL5 version
#


case `hostname` in
minervagpvm*)
   source /grid/fermiapp/minerva/software_releases/current_release/setup.sh
   PYTHON_DIR=`which python | sed -e 's;/bin/python;;'`
   export PYTHON_DIR
   DECLAREBITS="-4 -q python26"
   ;;
novagpvm02*|novagpvm06*)
   export ARCH=-m32
   DECLAREBITS="-f `ups flavor | sed -e 's/64bit//'`"
   ;;
*) 
   DECLAREBITS="-4"
   ;;
esac
