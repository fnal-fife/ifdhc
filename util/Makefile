SRCDIR=../../util/
VPATH=$(SRCDIR)
GEN=ifdh_version.h
SRC=$(notdir $(wildcard $(SRCDIR)*.cc *.cc *.c))
HDR=$(notdir $(wildcard $(SRCDIR)*.h *.h)
OBJ=$(addsuffix .o,$(basename $(SRC)))
TST=$(addsuffix -test,$(basename($SRC)))
DEFS=
TESTDEFS=-DUNITTEST
CXXFLAGS=-pedantic-errors -Wall -Wextra -Werror -fPIC -g $(DEFS) -std=c++11  -I../../util $(ARCH)
CFLAGS=-pedantic-errors -Wall -Wextra -fPIC -g $(DEFS) $(ARCH) -I../../util --std=c99


all: $(GEN) $(OBJ) $(TST)

clean:
	rm -f $(OBJ) *.a

# overwrite version.h with actual git version if git describe works
# otherwise leave it (i.e. for exported code)
ifdh_version.h: FORCE
	git describe --tags --match 'v*' 2>/dev/null && echo '#define IFDH_VERSION "'`git describe --tags --match 'v*'`'"' > $@.new && mv $@.new $@ || true

FORCE:

sha256-test: sha256.c
	${CC} -g -o $@ $(TESTDEFS) $(CFLAGS) $< 

utils-test: utils.cc WimpyConfigParser.o
	${CXX} -g -o $@ $(TESTDEFS) $(CXXFLAGS) $< WimpyConfigParser.o

%-test: %.cc WimpyConfigParser.o
	${CXX} -g -o $@ $(TESTDEFS) $(CXXFLAGS) $< utils.o WimpyConfigParser.o

%.o: %.cc
	${CXX} -c -o $@  $(CXXFLAGS) $< 

utils.o: utils.cc utils.h 
WebAPI.o: WebAPI.cc WebAPI.h 
