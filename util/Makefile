SRCDIR=../../util/
VPATH=$(SRCDIR)
GEN=ifdh_version.h
SRC=$(notdir $(wildcard $(SRCDIR)*.cc *.cc *.c))
HDR=$(notdir $(wildcard $(SRCDIR)*.h *.h)
OBJ=$(addsuffix .o,$(basename $(SRC)))
TST=$(addsuffix -test,$(basename($SRC)))
DEFS=

# header-only http client library, fetched from github at a tagged
# version, and checked against a known sha256 sum.
HTTPLIB_VERSION=v0.58.0
HTTPLIB_SHA256=aa14e7e7bd2703694e0a6b6855af3b8c406102ab1fc56ac905fe33619b31faa5
HTTPLIB_DIR=cpp-httplib-$(HTTPLIB_VERSION)
HTTPLIB_URL=https://raw.githubusercontent.com/yhirose/cpp-httplib/$(HTTPLIB_VERSION)/httplib.h
HTTPLIB_LIBS=-lssl -lcrypto

TESTDEFS=-DUNITTEST
CXXFLAGS=-pedantic-errors -Wall -Wextra -Werror -fPIC -g $(DEFS) -std=c++11  -I../../util -isystem $(HTTPLIB_DIR) $(ARCH) $(HTTPLIB_LIBS)
CFLAGS=-pedantic-errors -Wall -Wextra -fPIC -g $(DEFS) $(ARCH) -I../../util --std=c99 


all: $(HTTPLIB_DIR)/httplib.h $(GEN) $(OBJ) $(TST)

httplib.h: $(HTTPLIB_DIR)/httplib.h  
	cp $(HTTPLIB_DIR)/httplib.h  httplib.h

clean:
	rm -f $(OBJ) *.a

# also remove the downloaded cpp-httplib header
distclean: clean
	rm -rf cpp-httplib-*

$(HTTPLIB_DIR)/httplib.h:
	mkdir -p $(HTTPLIB_DIR)
	curl -sSfL --retry 3 -o $@.new $(HTTPLIB_URL)
	echo "$(HTTPLIB_SHA256)  $@.new" | (sha256sum -c - 2>/dev/null || shasum -a 256 -c -)
	mv $@.new $@

# overwrite version.h with actual git version if git describe works
# otherwise leave it (i.e. for exported code)
ifdh_version.h: FORCE
	git describe --tags --match 'v*' 2>/dev/null && echo '#define IFDH_VERSION "'`git describe --tags --match 'v*'`'"' > $@.new && mv $@.new $@ || true

FORCE:

sha256-test: sha256.c
	${CC} -g -o $@ $(TESTDEFS) $(CFLAGS) $< 

WebAPI-test: WebAPI.cc WebAPI.h $(HTTPLIB_DIR)/httplib.h utils.o WimpyConfigParser.o
	${CXX} -g -o $@ $(TESTDEFS) $(CXXFLAGS) $< utils.o WimpyConfigParser.o $(HTTPLIB_LIBS)

utils-test: utils.cc WimpyConfigParser.o
	${CXX} -g -o $@ $(TESTDEFS) $(CXXFLAGS) $< WimpyConfigParser.o

%-test: %.cc WimpyConfigParser.o
	${CXX} -g -o $@ $(TESTDEFS) $(CXXFLAGS) $< utils.o WimpyConfigParser.o

%.o: %.cc
	${CXX} -c -o $@  $(CXXFLAGS) $< 

utils.o: utils.cc utils.h 
WebAPI.o: WebAPI.cc WebAPI.h $(HTTPLIB_DIR)/httplib.h 
