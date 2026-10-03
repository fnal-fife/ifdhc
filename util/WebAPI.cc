#include "WebAPI.h"
#include <fstream>
#include <iostream>
#include <stdarg.h> 
#include <stdio.h>
#include <errno.h>
#include <iomanip>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <poll.h>
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
WebAPIException::WebAPIException( std::string message, std::string tag ) : logic_error(message + tag) {
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

static char *get_bearer_token() {
    static char tokenbuf[8192];
    static int have_token;
    int fd, res;

    switch(have_token) {
    case 1:
        // we already got it
        return tokenbuf;
    case 2:
        // we already didn't find it
        return NULL;
    default:
        // haven't looked yet...
        if (getenv("BEARER_TOKEN_FILE") != 0) {
             
            fd = open(getenv("BEARER_TOKEN_FILE"),O_RDONLY);

            if (fd >= 0) {
                res = read(fd, tokenbuf, 8192);
                close(fd);
                if (res > 512) {
                    // BEARER_TOKEN_FILE gave us an actual file that's
                    // a reasonable size, so go with it.
                    
                    have_token = 1;
                    // trim trailing newline...
                    if (tokenbuf[res-1] == '\n') {
                       tokenbuf[res-1] = 0;
                    }
                    return tokenbuf;
                }
            }
        }
        // remember we don't have one for next time
        have_token = 2;
        return 0;
    }
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
WebAPI::parseurl(std::string url, std::string http_proxy) {
     int i, j;                // string indexes
     WebAPI::parsed_url res;  // resulting pieces
     std::string part;        // partial url

     i = url.find_first_of(':');
     if (url[i+1] == '/' and url[i+2] == '/') {
        res.type  = url.substr(0,i);
     } else {
        throw(WebAPIException(url,"BadURL: has no slashes, must be full URL"));
     }
     if (http_proxy == "" && getenv("http_proxy")) {
         http_proxy = getenv("http_proxy");
     }
     if (res.type != "http" && res.type != "https" ) {
        throw(WebAPIException(url,"BadURL: only http: and https: supported"));
     }
     if (res.type == "http" && http_proxy != "") {
        // if we have a proxy, we connect to the proxy server, and
        // give the whole url for the path..
        res.path = url;
        i = http_proxy.find_first_of(':');
        if ( i < 0 ) {
             res.host = http_proxy;
             res.port = 8080;
        } else {
             res.host = http_proxy.substr(0,i);
             res.port = atol(http_proxy.substr(i+1,http_proxy.length()).c_str());
        }
     } else {
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
    }
    _debug && std::cerr << "parseurl: host " << res.host << " port: " << res.port << " path " << res.path << std::endl;
    _debug && std::cerr.flush();
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

WebAPI::WebAPI(std::string url, int postflag, std::string postdata, int maxretries, int timeout, std::string http_proxy, std::string auth_header)  {
     httplib::Client cli(url);
     httplib::Result res;
     httplib::Headers headers = {
        {"Accept", "application/json"},
        {"User-Agent", "my-app/1.0"},
     };
     char *tok;
        
     const char *content_type;

     cli.set_ca_cert_path("/etc/grid-security/certificates");
     cli.enable_server_certificate_verification(false);
     cli.set_max_timeout((time_t)( timeout * 1000));
     if ( http_proxy != "" ) {
         cli.set_proxy(http_proxy, 0 );
     }

     if (auth_header != "") {
         headers.emplace("Authorization", auth_header);
     } else if (0 != (tok = get_bearer_token())) {
         headers.emplace("Authorization", std::string("Bearer ") + tok);
     }

    _status = 0;
    int retries = 0;
    int totaltime = 0;
    while( retries < maxretries && ( _status < 200 || (_status > 205 && _status < 500))) {

         retries++;
         
         cli.set_default_headers(headers);

         if (postflag) {
             if ( postflag == 1) {
                  content_type =  "application/x-www-form-urlencoded";
             } else if ( postflag == 2) {
                  content_type =  "application/json";
             } else {
                  content_type =  "text/plain";
             }
             res = cli.Post(url, postdata, content_type );
        } else {
             res = cli.Get(url);
        }
        _status = res->status;
        if ( _status > 300 && _status < 305 ) {
            _debug && std::cerr << "Redirected: got back " << _status << ", Location:" << res->get_header_value("Location") << "\n";
            url =  res->get_header_value("Location");
        } else if ( _status < 200 || _status > 205 ) {
            _debug && std::cerr << "Error got back " << _status << ", Location:" << res->get_header_value("Location") << "\n";
            _debug && std::cerr << "Retrying after delay..";
        }           

         if (_status == 202 && retryafter > 0) {
            sleep(retryafter);
            totaltime += retryafter;
            retries--;          // it doesnt count if they told us to...
         }

         if (_status >= 500) {
            if (_debug) {
	        std::cerr << "50x error:\n=-=-=-=-=-=-=-=-=-=\n";
                std::cerr << res->body;
	        std::cerr << "\n=-=-=-=-=-=-=-=-=-=\nwaiting ...";
                std::cerr.flush();
            }
            retryafter = random() % (5 << retries);
            sleep(retryafter);
            totaltime += retryafter;
         }
         if (_status == 303) {
            //redirected, but to a GET...
            postdata = "";
            postflag = 0;
         }

         if ( _timeout > 0 && totaltime > (_timeout / 1000) ) {
            throw(WebAPIException(url, ": Timeout exceeded"));
         }
     }

     if (_status <  200 || _status >  209) {
        std::stringstream message;
        message << "\nHTTP-Status: " << _status << "\n";
        message << "Error text is:\n";
        while (_fromsite.getline(buf, 1024).gcount() > 0) {
    }
    _data.str(res->body);
}

int
WebAPI::getStatus() {
   return _status;
}

WebAPI::~WebAPI() {
   return;
}

void
test_WebAPI_fetchurl() {
   std::string line;


   if (0) {
   WebAPI ds("https://home.fnal.gov/~mengel/Ascii_Chart.html");

    std::cout << "ds.data().eof() is " << ds.data().eof() << std::endl;
    while(!ds.data().eof()) {
        getline(ds.data(), line);

        std::cout << "got line: " << line << std::endl;;
   }
   std::cout << "ds.data().eof() is " << ds.data().eof() << std::endl;
   }
   
   try {
      WebAPI ds3("https://samdev.fnal.gov:8483/sam/samdev/api/files/list?dims=defname%3Agen_cfg+++minus+++file_name+++c47fe3af-8fdb-4a5a-a110-3f3d52f3cfea-a.fcl+++minus++++file_name+++a9d1b4da-73ad-4c4f-8d72-c9e6507531b8-d.fcl&format=plain");
      while(!ds3.data().eof()) {
	    getline(ds3.data(), line);

	    std::cout << "got line: " << line << std::endl;;
      }
      for ( auto p = ds3._rcv_headers.begin(); p != ds3._rcv_headers.end(); p++  ) {
           std::cout << "header " << p->first << ": " <<  p->second << "\n";
      }
      std::string foo;
      foo = ds3._rcv_headers["Date"];
      std::cout << "extracted Date: " << foo << "\n";
      foo = ds3._rcv_headers["Content-Type"];
      std::cout << "extracted Content-Type: " << foo << "\n";
   } catch (WebAPIException &we) {
      std::cout << "WebAPIException: " << we.what() << std::endl;
   }
   return;

   WebAPI dsp("https://home.fnal.gov/~mengel/Ascii_Chart.html", 0, "", 10, -1, "squid.fnal.gov:3128");

    std::cout << "dsp.data().eof() is " << dsp.data().eof() << std::endl;
    while(!dsp.data().eof()) {
        getline(dsp.data(), line);

        std::cout << "got line: " << line << std::endl;;
   }
   std::cout << "dsp.data().eof() is " << dsp.data().eof() << std::endl;

   WebAPI ds2("https://home.fnal.gov/~mengel/Ascii_Chart.html");

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
   try {
      // try a webpage that takes 10 seconds with a 5 second timeout..
      WebAPI ds7("http://deelay.me/10000/https://home.fnal.gov/~mengel/AsciiChart.html", 0, "", 10, 5);
   } catch (WebAPIException &we) {
      std::cout << "WebAPIException: " << we.what() << std::endl;
   }
}

void
test_WebAPI_leakcheck() {
   std::string line;

   for(int  i=0; i< 2048; i++) {
       WebAPI ds("https://home.fnal.gov/~mengel/Ascii_Chart.html");
       while(!ds.data().eof()) {
            getline(ds.data(), line);
       }
   }
}
 
}
#ifdef UNITTEST

int
main() {
   ifdh_util_ns::WebAPI::_debug = 1;
   test_encode();
   test_WebAPI_fetchurl();
   // test_WebAPI_leakcheck();
   return 0;
}
#endif
