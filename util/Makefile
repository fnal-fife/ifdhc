SRCDIR=../../util/
VPATH=$(SRCDIR)
GEN=ifdh_version.h
SRC=$(notdir $(wildcard $(SRCDIR)*.cc *.cc))
HDR=$(notdir $(wildcard $(SRCDIR)*.h *.h)
OBJ=$(addsuffix .o,$(basename $(SRC)))
TST=$(addsuffix -test,$(basename($SRC)))
DEFS=
TESTDEFS=-DUNITTEST
CXXFLAGS=-pedantic-errors -Wall -Wextra -Werror -fPIC -g $(DEFS) $(ARCH) -I../../util


all: $(GEN) $(OBJ) $(TST)

clean:
	rm -f $(OBJ) *.a

# overwrite version.h with actual git version if git describe works
# otherwise leave it (i.e. for exported code)
ifdh_version.h: FORCE
	echo '#define IFDH_VERSION "'`git describe --tags --match 'v*'`'"' > $@.new && mv $@.new $@ || true

FORCE:

%-test: %.cc
	g++ -o $@ $(TESTDEFS) $(CXXFLAGS) $< utils.o

%.o: %.cc
	g++ -c -o $@  $(CXXFLAGS) $< 
utils.o: utils.cc utils.h 
WebAPI.o: WebAPI.cc WebAPI.h 
