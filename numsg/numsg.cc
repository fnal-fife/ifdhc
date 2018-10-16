#include "numsg.h"
#include "utils.h"
#include <sstream>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include "ifdh_version.h"

namespace ifdh_util_ns {

numsg *numsg::_singleton = 0;

numsg::numsg(const char *jobname, const char *host, int port, int parentflag) : 
	_sa(host, port, parentflag), 
	_jobname(jobname?jobname:(getenv("NU_LOG_TAG")?getenv("NU_LOG_TAG"):"ifdh_log")),
	_cur_state("boot"), 
	_old_states(), 
        _facility(17),
        _severity(5)
{
    _jobname.append("/");
    _jobname.append(IFDH_VERSION);
    _jobname.append("/");
    _jobname.append(getexperiment());
}

numsg::~numsg() {
    new_state("exiting");
}

numsg *
numsg::init(const char *jobname, int parentflag) {
    char hostbuf[512];
    char *host;
    char *pcolon;
    int port;
    
    host = getenv("NU_LOG_HOST");
    if (host) { 
	strncpy(hostbuf, host, 511);
	if (0 != (pcolon = strchr(hostbuf,':'))) {
	    port = atoi(pcolon + 1);
	    *pcolon = 0; 
	} else {
	    port = 514;
	}
    } else {
        strcpy(hostbuf, "fifelog.fnal.gov");
        port = 5140;
    }
    if (jobname == 0 && getenv("NU_LOG_TAG"))  {
        jobname = getenv("NU_LOG_TAG");
    }
    numsg::_singleton = new numsg(jobname, hostbuf, port, parentflag);
    return numsg::_singleton;
}

numsg *
numsg::getMsg() {
    return numsg::_singleton;
}

void
numsg::new_state(const char *state) {
    std::stringstream st;
    _cur_state = state;
    st <<  "State: " << _cur_state;
    _sa.send( _facility, _severity, _jobname.c_str(), st.str().c_str());
}

void
numsg::start(const char *what) {
    _starttime = time(0);
    _old_states.push_front(_cur_state);
    new_state(what);
}

void
numsg::printf(const char *fmt, ...) {
    static char buf[1024];
    va_list va;
    va_start(va, fmt);
    vsnprintf(buf, 1024, fmt, va);
    _sa.send( _facility, _severity, _jobname.c_str(), buf);
}

void
numsg::finish(const char *what) {
    std::stringstream st;
    std::string s;

    printf("Finished: %s -- elapsed %d sec.", what, (time(0) - _starttime));
    new_state(_old_states.front().c_str());
    _old_states.pop_front();
}

}
#ifdef UNITTEST
int
main() {
    /*  putenv("NU_LOG_HOST=localhost:514"); */
    putenv((char *)"NU_LOG_TAG=testjob");
    //nm = numsg::init("testjob");
    (void)numsg::init(0);
    numsg::getMsg()->new_state("foo");
    numsg::getMsg()->new_state("bar");
    numsg::getMsg()->start("laundry");
    sleep(2);
    numsg::getMsg()->printf("test %s with %d things", "washer", 25);
    sleep(2);
    numsg::getMsg()->start("dishes");
    sleep(2);
    numsg::getMsg()->finish("dishes");
    sleep(2);
    numsg::getMsg()->finish("laundry");
    numsg::getMsg()->printf("Exiting:");
}
#endif

