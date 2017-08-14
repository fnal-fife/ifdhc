#include "WebAPI.h"
#include <fstream>
#include <iostream>
#include <stdarg.h> 
#include <stdio.h>
#include <errno.h>
#include <iomanip>
#include <string.h>
#include <stdlib.h>
#include <ext/stdio_filebuf.h>
#include <sys/types.h>
#include <unistd.h>
#include "utils.h"
#include <pwd.h>
#include "ifdh_version.h"
#include <sys/wait.h>
#include <fcntl.h>


namespace ifdh_util_ns {
// debug flag
int WebAPI::_debug(0);

//
// initialize exception
// - zero fill whole structure
// - set message and tag fields
// this works if we're subclassed from GaudiException or our
// SimpleException...
//
WebAPIException::WebAPIException( std::string message, std::string tag ) throw() : logic_error(message + tag) {
   ;
}

std::string
WebAPI::encode(std::string s) {
    std::string res("");
    static char digits[] = "0123456789abcdef";
    
    for(size_t i = 0; i < s.length(); i++) {
        if (
              (s[i] >= 'a' && s[i] <= 'z') ||
              (s[i] >= 'A' && s[i] <= 'Z') ||
              (s[i] >= '0' && s[i] <= '9') || s[i] == '_' ) {
            res.append(1, s[i]);
         } else {
            res.append(1, '%');
            res.append(1, digits[(s[i]>>4)&0xf]);
            res.append(1, digits[s[i]&0xf]);
         }
    }
    return res;
}	

void
test_encode() {
    std::string s="testing(again'for'me)";
    std::cout << "converting: " << s << " to: " << WebAPI::encode(s) << "\n";
}
// parseurl(url)
//   parse a url into 
//   * type/protocol, 
//   * host
//   * port
//   * path
//   so that it can be fetched directly

WebAPI::parsed_url 
WebAPI::parseurl(std::string url) throw(WebAPIException) {
     int i, j;                // string indexes
     WebAPI::parsed_url res;  // resulting pieces
     std::string part;        // partial url

     i = url.find_first_of(':');
     if (url[i+1] == '/' and url[i+2] == '/') {
        res.type  = url.substr(0,i);
     } else {
        throw(WebAPIException(url,"BadURL: has no slashes, must be full URL"));
     }
     if (res.type != "http" && res.type != "https" ) {
        throw(WebAPIException(url,"BadURL: only http: and https: supported"));
     }
     part = url.substr(i+3);
     i = part.find_first_of(':');
     j = part.find_first_of('/');
     if( i < 0 || i > j) {
	 // no port number listed, dedault to 80 or 443
         res.host = part.substr(0,j);
         res.port = (res.type == "http") ?  80 : 443;
     } else {
	 res.host = part.substr(0,i);
         res.port = atol(part.substr(i+1,j-i).c_str());
    }
    res.path = part.substr(j);
    return res;
}

// we need lots of system network bits
// to make a network connection...
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>



// fetch a URL, opening a filestream to the content
// we do klugy looking things here to directly return
// the network connection, rather than saving he data
// in a file and returning that.

WebAPI::WebAPI(std::string url, int postflag, std::string postdata) throw(WebAPIException) {
     int s = -1;		// unix socket file descriptor
     WebAPI::parsed_url pu;     // parsed url.
     // struct sockaddr_storage server; // connection address struct
     struct addrinfo *addrp;   // getaddrinfo() result
     struct addrinfo *addrf;   // getaddrinfo() result, to free later
     static char buf[512];      // buffer for header lines
     int optval, optlen;
     int retries;
     int res;
     int retryafter = -1;
     int redirect_or_retry_flag = 1;
     int hcount;
     int connected;
     int totaltime = 0;
     int timeoutafter = -1;

     _pid = 0;
     __gnu_cxx::stdio_filebuf<char> *buf_out = 0;
     std::string method(postflag?"POST ":"GET ");

     _debug && std::cerr << "fetchurl: " << url << std::endl;
     _debug && std::cerr.flush();
     retries = 0;

     if (getenv("IFDH_WEB_TIMEOUT")) { 
          timeoutafter = atoi(getenv("IFDH_WEB_TIMEOUT"));
     }

     while( redirect_or_retry_flag ) {
         hcount = 0;
         _status = 500;
         retries++;

         // note that this retry limit includes 303 redirects, 503 errors, DNS fails, and connect errors...
	 if (retries > 10) {
	     throw(WebAPIException(url,"FetchError: Retry count exceeded"));
	 }

         pu = parseurl(url);

         if (pu.type == "http") {
             struct addrinfo hints; 
             char portbuf[10];

             memset(&hints, 0, sizeof(hints));
             hints.ai_socktype = SOCK_STREAM;
             hints.ai_family = AF_UNSPEC;
             hints.ai_flags = AI_CANONNAME;
             sprintf( portbuf, "%d", pu.port);
             // connect directly
             res = getaddrinfo(pu.host.c_str(), portbuf, &hints, &addrp);
             addrf = addrp;
	     if (res != 0) {
		 _debug && std::cerr << "getaddrinfo failed , waiting ..." << retries << std::endl;
		 _debug && std::cerr.flush();
		 sleep(retries);
                 totaltime += retries;
		 continue;
	     }

             connected = 0;
	     while ( addrp && !connected) {

		 _debug && std::cerr << "looking up host " << pu.host << " got " << (addrp->ai_canonname?addrp->ai_canonname:"(null)") <<  " type: " << addrp->ai_family << "\n";
		 _debug && std::cerr.flush();

		 s = socket(addrp->ai_family, addrp->ai_socktype,0);

                 // turn on keepalive, so we know if we lose the
                 // other end...
                 optval = 1;
                 optlen = sizeof(optval);
                 setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen);

		 if (connect(s, addrp->ai_addr,addrp->ai_addrlen) < 0) {
                     _debug && std::cerr << "connect failed: errno = " << errno << "\n";
		     addrp = addrp->ai_next;
		 } else {
                     _debug && std::cerr << "connect succeeded\n";
		     connected = 1;
		 }

	     }
             freeaddrinfo(addrf);

             if (!connected) {
		 _debug && std::cerr << " all connects failed , waiting ...";
                 _debug && std::cerr.flush();
		 sleep(5 << retries);
		 _debug && std::cerr << "retrying ...\n";
                continue;
	     }

	     // start of black magic -- use stdio_filebuf class to 
	     // attach to socket...
	     _buf_in = new  __gnu_cxx::stdio_filebuf<char> (dup(s), std::fstream::in|std::fstream::binary); 
	     if (!_buf_in) {
		throw(WebAPIException(url,"MemoryError: new failed"));
	     }
	     _fromsite.std::ios::rdbuf(_buf_in);

	     buf_out = new  __gnu_cxx::stdio_filebuf<char>(dup(s), std::fstream::out|std::fstream::binary); 
	     if (!buf_out) {
		throw(WebAPIException(url,"MemoryError: new failed"));
	     }
	     _tosite.std::ios::rdbuf(buf_out);
	     // end of black magic

             if (s != -1) {
	        close(s);	     // don't need original dd anymore...
             }
         } else if (pu.type == "https") {

            // XXX How do we detect/retry https fails?

            int inp[2], outp[2], pid;
            pipe(inp);
            pipe(outp);
            if (0 == (pid = fork())) {
                // child -- run openssl s_client
                const char *proxy = getenv("X509_USER_PROXY");
                std::stringstream hostport;

                hostport << pu.host << ":" << pu.port;

                _debug && std::cerr << "openssl"<< ' ' << "s_client"<< ' ' << " -CApath /etc/grid-security/certificates" << ' ' << "-connect"<< ' ' << hostport.str().c_str() << " -quiet"; 
                if (proxy && _debug) {
                     // from https://wiki.nikhef.nl/grid/How_to_handle_OpenSSL_and_not_get_hurt_using_the_CLI#Using_proxy_certificates_and_s_client
		    std::cerr << " -key " << proxy << " -cert "<< proxy << " -CAfile " << proxy ;
                }

                std::cerr.flush();

                // fixup file descriptors so our in/out are pipes
                close(0);      
                dup(inp[0]);   
                close(1);
                dup(outp[1]);
                // send stderr to /dev/null -- get rid of annoying
                // validation messages.  Might lose some real errors,
                // but...
                close(2);
                open("/dev/null",O_RDONLY);

                close(inp[0]); close(inp[1]); 
                close(outp[0]);close(outp[1]);

                // run openssl...
                if (proxy) {
			    execlp("openssl", "s_client", "-CApath", "/etc/grid-security/certificates/",  "-connect", hostport.str().c_str(),  "-quiet",  "-cert", proxy, "-key", proxy, "-CAfile", proxy,  (char *)0);
                } else {
                    execlp("openssl", "s_client", "-CApath", "/etc/grid-security/certificates/", "-connect", hostport.str().c_str(),  "-quiet",  (char *)0);
                }
                exit(-1);
            } else {
                // parent, fix up pipes, make streams
                _pid = pid;
                close(inp[0]);  
                close(outp[1]);  
	        _buf_in = new  __gnu_cxx::stdio_filebuf<char> (outp[0], std::fstream::in|std::fstream::binary); 
		 if (!_buf_in) {
		    throw(WebAPIException(url,"MemoryError: new failed"));
		 }
	         _fromsite.std::ios::rdbuf(_buf_in);

                 buf_out = new  __gnu_cxx::stdio_filebuf<char>(inp[1], std::fstream::out|std::fstream::binary);
		 if (!buf_out) {
		    throw(WebAPIException(url,"MemoryError: new failed"));
		 }
		 _tosite.std::ios::rdbuf(buf_out);
            }
         } else {
            throw(WebAPIException(url,"BadURL: only http: and https: supported"));
         }

         std::string user;
         struct passwd *ppasswd = getpwuid(getuid());

         if(ppasswd) 
            user = ppasswd->pw_name;
         else if (getenv("GRID_USER"))
            user = getenv("GRID_USER");
         else if (getenv("USER"))
            user = getenv("USER");
         else
            user = "unknown";

         char hostbuf[512];
         gethostname(hostbuf, 512);

	 // now some basic http protocol
	 _tosite << method << pu.path << " HTTP/1.0\r\n";
	 _debug && std::cerr << "sending: "<< method << pu.path << " HTTP/1.0\r\n";
	 _tosite << "Host: " << pu.host << ":" << pu.port <<"\r\n";
	 _debug && std::cerr << "sending header: " << "Host: " << pu.host << "\r\n";
	 _tosite << "From: " << user << "@" << hostbuf  <<"\r\n";
	 _debug && std::cerr << "sending header: " << "From: " << ppasswd->pw_name << "@" << hostbuf << "\r\n";
	 _tosite << "User-Agent: " << "WebAPI/" << IFDH_VERSION << "/Experiment/" << getexperiment() << "\r\n";
	 _debug && std::cerr << "sending header: " << "User-Agent: " << "WebAPI/" << IFDH_VERSION << "/Experiment/" << getexperiment() << "\r\n";
         if (postflag) {

             if ( postflag == 1) {
                  _debug && std::cerr << "sending header:" << "Content-Type: application/x-www-form-urlencoded\r\n";
                 _tosite << "Content-Type: application/x-www-form-urlencoded\r\n";
             } else if ( postflag == 2) {
                  _debug && std::cerr << "sending header:" <<  "Content-Type: application/json\r\n";
                 _tosite << "Content-Type: application/json\r\n";
             } else {
                  _debug && std::cerr << "sending header:" << "Content-Type: text/plain\r\n";
                 _tosite << "Content-Type: text/plain\r\n";
             }
	      _debug && std::cerr << "sending header:"<< "Content-Length: " << postdata.length() << "\r\n";
             _tosite << "Content-Length: " << postdata.length() << "\r\n";
             _debug && std::cerr << "sending post data: " << postdata << "\n" << "length: " << postdata.length() << "\n"; 
	     _tosite << "\r\n";
             _tosite << postdata;
         } else {
	     _tosite << "\r\n";
         }
	 _tosite.flush();

         _debug && std::cerr << "sent request\n";

	 do {
	    _fromsite.getline(buf, 512);
            hcount++;

	    _debug && std::cerr << "got header line " << buf << "\n";

	    if (strncmp(buf,"HTTP/1.", 7) == 0) {
		_status = atol(buf + 8);
	    }

	    if (strncmp(buf, "Retry-After: ", 13) == 0) {
                retryafter = atol(buf + 13);
            }

	    if (strncmp(buf, "Location: ", 10) == 0) {
		if (buf[strlen(buf)-1] == '\r') {
		    buf[strlen(buf)-1] = 0;
		}
		url = buf + 10;
	    }

	 } while (_fromsite.gcount() > 2 || hcount < 3); // end of headers is a blank line

	 _debug && std::cerr << "http status: " << _status << std::endl;

	 _tosite.close();
         if (buf_out) 
             delete buf_out;

         if (_status == 202 && retryafter > 0) {
            sleep(retryafter);
            totaltime += retryafter;
            retries--;          // it doesnt count if they told us to...
         }

         if (_status >= 500) {
	    _debug && std::cerr << "50x error , waiting ...";
            retryafter = random() % (5 << retries);
            sleep(retryafter);
            totaltime += retryafter;
         }
         if (_status == 303) {
            //redirected, but to a GET...
            postflag = 0;
         }

         if ((_status < 301 || _status > 309) && _status < 500 && _status != 202 ) {
            redirect_or_retry_flag = 0;
	 } else {
	     int wstatus;
	     if (_pid) {
	        (void) waitpid(_pid,&wstatus,0);
	        _pid = 0;
	     }
	     // we're going to redirect/retry again, so close the _fromsite side
	     _fromsite.close();
             
             delete _buf_in;
         }

         if ( timeoutafter > 0 && totaltime > timeoutafter ) {
            throw(WebAPIException(url, ": Timeout exceeded"));
         }
     }

     if (_status <  200 || _status >  209) {
        std::stringstream message;
        message << "\nHTTP-Status: " << _status << "\n";
        message << "Error text is:\n";
        while (_fromsite.getline(buf, 512).gcount() > 0) {
	    message << buf << "\n";
        }
        throw(WebAPIException(url,message.str()));
     }    
}

int
WebAPI::getStatus() {
   return _status;
}

WebAPI::~WebAPI() {
    int wstatus;
    if (_pid) {
        (void) waitpid(_pid,&wstatus,0);
        (void) waitpid(-1,&wstatus,WNOHANG);
    }
    _tosite.close();
    _fromsite.close();
    delete _buf_in;
}

void
test_WebAPI_fetchurl() {
   std::string line;


   WebAPI ds("http://www-oss.fnal.gov/~mengel/Ascii_Chart.html");

    std::cout << "ds.data().eof() is " << ds.data().eof() << std::endl;
    while(!ds.data().eof()) {
        getline(ds.data(), line);

        std::cout << "got line: " << line << std::endl;;
   }
   std::cout << "ds.data().eof() is " << ds.data().eof() << std::endl;
   ds.data().close();

   WebAPI ds2("http://home.fnal.gov/~mengel/Ascii_Chart.html");

    while(!ds2.data().eof()) {
        getline(ds2.data(), line);

        std::cout << "got line: " << line << std::endl;;
   }
   std::cout << "ds.data().eof() is " << ds2.data().eof() << std::endl;
   std::cout << "ds.getStatus() is " << ds2.getStatus() << std::endl;

   WebAPI dsgoog("http://www.google.com/");

    std::cout << "ds.data().eof() is " << dsgoog.data().eof() << std::endl;
    while(!dsgoog.data().eof()) {
        getline(dsgoog.data(), line);

        std::cout << "got line: " << line << std::endl;;
   }
   std::cout << "ds.data().eof() is " << dsgoog.data().eof() << std::endl;
   dsgoog.data().close();


   try {
      WebAPI ds3("https://computing.fnal.gov/");
      while(!ds3.data().eof()) {
	    getline(ds3.data(), line);

	    std::cout << "got line: " << line << std::endl;;
      }
   } catch (WebAPIException &we) {
      std::cout << "WebAPIException: " << we.what() << std::endl;
   }

   try {
      WebAPI ds4("http://nosuch.fnal.gov/~mengel/Ascii_Chart.html");
   } catch (WebAPIException &we) {
      std::cout << "WebAPIException: " << we.what() << std::endl;
   }
   try {
      WebAPI ds5("borked://nosuch.fnal.gov/~mengel/Ascii_Chart.html");
   } catch (WebAPIException &we) {
      std::cout  << "WebAPIException: " << we.what() << std::endl;
   }
   try {
      WebAPI ds6("http://www.fnal.gov/nosuchdir/nosuchfile.html");
   } catch (WebAPIException &we) {
      std::cout << "WebAPIException: " << we.what() << std::endl;
   }
}

}
#ifdef UNITTEST

int
main() {
   ifdh_util_ns::WebAPI::_debug = 1;
   test_encode();
   test_WebAPI_fetchurl();
}
#endif
