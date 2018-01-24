#include "../util/SyslogAPI.h"
#include <string>
#include <list>

namespace ifdh_util_ns {

class numsg {
     SyslogAPI _sa; 		// SyslogAPI object to send with
     std::string _jobname;	// current job name
     std::string _cur_state;    // state we're in
     std::list<std::string> _old_states; // states we pushed down
     int _facility;		// Syslog facility/severity
     int _severity;
     time_t _starttime;		// last time we start()-ed something
                                // so we can give elapsed time in finish()
     numsg(const char * jobname, const char *host, int port, int parentflag=0);
				// private constructor for singleton
     static numsg *_singleton;
public:
     static numsg *init(const char *jobname, int parentflag=0);
				// sets things up with jobname, etc.
     static numsg *getMsg();
				// find the singleton for use
     ~numsg();			// clean up
     void new_state(const char *state);		// log a State: message
     void start(const char *state);		// log a Starting blah message
     void finish(const char *state);		// log a Finishing blah message
						//  with elapsed time
     void printf(const char *fmt, ...);		// general printf method
};

}

using namespace ifdh_util_ns;
