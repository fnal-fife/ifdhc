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

SyslogAPI::SyslogAPI(char *desthost, int destport, int parentflag): _parentflag(parentflag) {
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
}

SyslogAPI::~SyslogAPI() {
     close(_socket);
}

ssize_t
SyslogAPI::send( int facility, int severity, const char *tag, const char *msg) {

     struct hostent *phe;
     char hostbuf[512];
     std::stringstream st;
     int pri = facility * 8 + severity;
     time_t t = time(0);
     char *date = ctime(&t)+4;
     date[15] = 0;

     gethostname(hostbuf, 512);
     phe = gethostbyname(hostbuf);

     st << "<" << pri << ">";
     st << date;
     if (phe) {
         st << ' ' << phe->h_name  << ' ' ;
     } else if (hostbuf[0]) {
         st << ' ' << hostbuf  << ' ' ;
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
main() {

   SyslogAPI s("localhost");
   s.send(17, 2, "SyslogAPI", "Client test message");
}
#endif
