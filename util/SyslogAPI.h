#ifndef SYSLOG_API_H
#define SYSLOG_API_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>

namespace ifdh_util_ns {

class SyslogAPI {
    static int _debug;			// debugging flag
    int _socket;			// socket we send through
    struct sockaddr_storage _destaddr;	// our destination host/port
    int _parentflag;			// use our parents pid rather than ours
    char _hostbuf[512];
    struct hostent *_phe;               // our local host entry for FQDN
public:
    SyslogAPI(const char *desthost, int destport = 514, int parentflag = 0);
    ~SyslogAPI();
    ssize_t send( int facility, int severity, const char *tag, const char *msg);
					// actually send a syslog message
};

}

using namespace ifdh_util_ns;
#endif //SYSLOG_API_H
