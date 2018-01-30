#include "SyslogAPI.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

namespace ifdh_util_ns {

int SyslogAPI::_debug = 0;

SyslogAPI::SyslogAPI(const char *desthost, int destport, int parentflag): _parentflag(parentflag) {
     int res;
     struct sockaddr_in sa;
     struct addrinfo hints, *paddrs; 
     char portbuf[10];

     memset(&hints, 0, sizeof(hints));
     hints.ai_socktype = SOCK_DGRAM;
     hints.ai_family = AF_UNSPEC;
     hints.ai_flags = AI_CANONNAME;

     sprintf(portbuf, "%d", destport);

     res = getaddrinfo(desthost, portbuf, &hints, &paddrs);
     if (res != 0) 
         return;
   
     memset((char *)&sa, 0, sizeof(sa));
     sa.sin_family = paddrs->ai_family;

     _socket = socket(paddrs->ai_family, paddrs->ai_socktype, 0);
     if (_socket < 0) 
         return;
     bind(_socket, (struct sockaddr *)&sa, sizeof(sa));

     memcpy((char *)&_destaddr, paddrs->ai_addr, paddrs->ai_addrlen);
     freeaddrinfo(paddrs);

     gethostname(_hostbuf, 512);
     _phe = gethostbyname(_hostbuf);
}

SyslogAPI::~SyslogAPI() {
     close(_socket);
}

ssize_t
SyslogAPI::send( int facility, int severity, const char *tag, const char *msg) {

     char datebuf[24];
     std::stringstream st;
     int pri = facility * 8 + severity;
     time_t t = time(0);
     strftime(datebuf, 24, "%Y-%m-%dT%H:%M:%SZ",gmtime(&t));


     st << "<" << pri << ">";
     st << datebuf;
     if (_phe) {
         st << ' ' << _phe->h_name  << ' ' ;
     } else if (_hostbuf[0]) {
         st << ' ' << _hostbuf  << ' ' ;
     } else {
         st << " unknown_host " ;
     }
     st << tag << "[" << (_parentflag ? getppid() : getpgrp())  << "]: ";
     st << msg;

     return sendto(_socket, 
		st.str().c_str(), 
		st.str().length(), 
                0,
		(struct sockaddr*)&_destaddr, 
		sizeof(_destaddr));
}

}
#ifdef UNITTEST
int
main() {

   SyslogAPI s("localhost", 514);
   s.send(17, 2, "SyslogAPI", "Client test message");
   return 0;
}
#endif
