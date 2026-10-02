#include "WebAPI.h"
#include <iostream>
#include <chrono>
#include <memory>
#include <vector>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <pwd.h>
#include "utils.h"
#include "ifdh_version.h"

// header-only http(s) client from https://github.com/yhirose/cpp-httplib
// fetched by the Makefile.
#define CPPHTTPLIB_OPENSSL_SUPPORT 1
#include "httplib.h"


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
    int fd, res;

    // haven't looked yet...
    //
    // if BEARER_TOKEN_FILE isn't set and the default location exists, set BEARER_TOKEN_FILE to it
    if ( 0 == access(default_token_file().c_str(), R_OK) && getenv("BEARER_TOKEN_FILE") == 0) {
       setenv("BEARER_TOKEN_FILE", default_token_file().c_str(), 1);
    }

    // now if BEARER_TOKEN_FILE is set, fetch the token
    if (getenv("BEARER_TOKEN_FILE") != 0) {

        fd = open(getenv("BEARER_TOKEN_FILE"),O_RDONLY);

        if (fd >= 0) {
            res = read(fd, tokenbuf, 8191);
            close(fd);
            if (res > 512) {
                // BEARER_TOKEN_FILE gave us an actual file that's
                // a reasonable size, so go with it.
                tokenbuf[res] = 0;

                // trim trailing newline...
                if (tokenbuf[res-1] == '\n') {
                   tokenbuf[res-1] = 0;
                }
                return tokenbuf;
            }
        }
    }
    return 0;
}

void
test_encode() {
    std::string s="testing(again'for'me)";
    std::cout << "converting: " << s << " to: " << WebAPI::encode(s) << "\n";
}

// split_url(url)
//   split a url into type/protocol, host, port and path
//   without any proxy considerations
static WebAPI::parsed_url
split_url(std::string url) {
     size_t i, j;             // string indexes
     WebAPI::parsed_url res;  // resulting pieces
     std::string part;        // partial url

     i = url.find("://");
     if (i == std::string::npos || i == 0) {
        throw(WebAPIException(url,"BadURL: has no slashes, must be full URL"));
     }
     res.type = url.substr(0,i);
     if (res.type != "http" && res.type != "https" ) {
        throw(WebAPIException(url,"BadURL: only http: and https: supported"));
     }
     part = url.substr(i+3);
     j = part.find_first_of("/?");
     i = part.find_first_of(':');
     if( i == std::string::npos || i > j) {
         // no port number listed, default to 80 or 443
         res.host = part.substr(0,j);
         res.port = (res.type == "http") ?  80 : 443;
     } else {
         res.host = part.substr(0,i);
         res.port = atol(part.substr(i+1,j-i-1).c_str());
     }
     if (j == std::string::npos) {
         res.path = "/";
     } else if (part[j] == '?') {
         res.path = "/" + part.substr(j);
     } else {
         res.path = part.substr(j);
     }
     return res;
}

// parse a proxy setting like "host:port" or "http://host:port/"
// into its host and port.
static void
split_proxy(std::string proxy, std::string &host, int &port) {
     size_t i;

     i = proxy.find("://");
     if (i != std::string::npos) {
         proxy = proxy.substr(i+3);
     }
     i = proxy.find('/');
     if (i != std::string::npos) {
         proxy = proxy.substr(0,i);
     }
     i = proxy.find(':');
     if (i == std::string::npos) {
         host = proxy;
         port = 8080;
     } else {
         host = proxy.substr(0,i);
         port = atol(proxy.substr(i+1).c_str());
     }
}

// parseurl(url)
//   parse a url into
//   * type/protocol,
//   * host
//   * port
//   * path
//   so that it can be fetched directly; for http: urls with a proxy
//   the host and port are the proxy, and the path is the whole url.

WebAPI::parsed_url
WebAPI::parseurl(std::string url, std::string http_proxy) {
     WebAPI::parsed_url res;  // resulting pieces

     res = split_url(url);
     if (http_proxy == "" && getenv("http_proxy")) {
         http_proxy = getenv("http_proxy");
     }
     if (res.type == "http" && http_proxy != "") {
        // if we have a proxy, we connect to the proxy server, and
        // give the whole url for the path..
        res.path = url;
        split_proxy(http_proxy, res.host, res.port);
     }
    _debug && std::cerr << "parseurl: host " << res.host << " port: " << res.port << " path " << res.path << std::endl;
    _debug && std::cerr.flush();
    return res;
}

// remove "." and ".." segments from a path, as per RFC 3986 5.2.4
static std::string
remove_dot_segments(std::string path) {
    std::string query;
    size_t q = path.find('?');
    if (q != std::string::npos) {
        query = path.substr(q);
        path = path.substr(0, q);
    }
    std::vector<std::string> segs;
    size_t pos = 1;
    while (pos <= path.size()) {
        size_t next = path.find('/', pos);
        if (next == std::string::npos) next = path.size();
        std::string seg = path.substr(pos, next - pos);
        bool last = (next == path.size());
        if (seg == "..") {
            if (!segs.empty()) segs.pop_back();
            if (last) segs.push_back("");
        } else if (seg == ".") {
            if (last) segs.push_back("");
        } else {
            segs.push_back(seg);
        }
        pos = next + 1;
    }
    std::string res;
    for (auto &seg : segs) {
        res += "/" + seg;
    }
    return (res.empty() ? "/" : res) + query;
}

// figure out where a Location: header sends us, relative to
// the url we just fetched
static std::string
redirect_url(const WebAPI::parsed_url &pu, std::string loc) {
    if (loc.find("://") != std::string::npos) {
        // full url
        return loc;
    }
    std::string base = pu.type + "://" + pu.host + ":" + std::to_string(pu.port);
    if (loc[0] == '/') {
        // absolute path on the same server
        return base + remove_dot_segments(loc);
    }
    // relative path, replace after the last / of the path
    std::string path = pu.path.substr(0, pu.path.find('?'));
    return base + remove_dot_segments(path.substr(0, path.rfind('/') + 1) + loc);
}

// fetch a URL, reading the content into the data() stream.
//
// retries (which include redirects) are done on connection failures,
// 50x errors, and 202 responses with a Retry-After header.

WebAPI::WebAPI(std::string url, int postflag, std::string postdata, int maxretries, int timeout, std::string http_proxy, std::string auth_header)  {
     typedef std::chrono::steady_clock clock;
     clock::time_point start = clock::now();
     WebAPI::parsed_url pu;     // parsed url.
     httplib::Headers headers;  // headers we send
     std::string body;          // response body
     std::string user;
     std::string auth_name, auth_value;
     struct passwd *ppasswd = getpwuid(getuid());
     char hostbuf[512];
     const char *x509_proxy;
     const char *tok;
     int retries = 0;
     int retryafter;
     long remaining = -1;
     std::string lasterr;       // reason for the last retry, for timeout messages

     // sleep before a retry, but not past our timeout
     auto nap = [&](long secs) {
         if (_timeout > 0) {
             long left = _timeout - std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - start).count();
             if (secs * 1000 > left) {
                 throw(WebAPIException(url, ": Timeout exceeded" + lasterr));
             }
         }
         sleep(secs);
     };

     _status = 500;
     _timeout = timeout;
     if (_timeout == -1 && getenv("IFDH_WEB_TIMEOUT")) {
          _timeout = atoi(getenv("IFDH_WEB_TIMEOUT")) * 1000;
     }
     _timeout != -1 && _debug && std::cerr << "timeout: " << _timeout << "\n";

     _debug && std::cerr << "fetchurl: " << url << std::endl;
     _debug && std::cerr.flush();

     if (getenv("GRID_USER"))
        user = getenv("GRID_USER");
     else if (getenv("USER"))
        user = getenv("USER");
     else if(ppasswd)
        user = ppasswd->pw_name;
     else
        user = "unknown_user";

     gethostname(hostbuf, sizeof(hostbuf));
     hostbuf[sizeof(hostbuf)-1] = 0;

     if (!auth_header.empty()) {
          size_t colon = auth_header.find(':');
          if (colon == std::string::npos) {
              throw(WebAPIException(auth_header, ": BadHeader: expected 'Name: value'"));
          }
          auth_name = auth_header.substr(0, colon);
          size_t vstart = auth_header.find_first_not_of(" \t", colon + 1);
          auth_value = (vstart == std::string::npos) ? "" : auth_header.substr(vstart);
     }

     while( true ) {

         retries++;

         // note that this retry limit includes 30x redirects, 50x errors, DNS fails, and connect errors...
	 if (retries > maxretries+1) {
             // don't lose debug messages..
             std::cerr << "retries " << retries << " maxretries " << maxretries << "\n";
             std::cerr.flush();
	     throw(WebAPIException(url,"FetchError: Retry count exceeded"));
	 }

         if ( _timeout > 0 ) {
             remaining = _timeout - std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - start).count();
             if ( remaining <= 0 ) {
                 throw(WebAPIException(url, ": Timeout exceeded" + lasterr));
             }
         }

         pu = split_url(url);

         std::string base = pu.type + "://" + pu.host + ":" + std::to_string(pu.port);
         std::unique_ptr<httplib::Client> cli;

         x509_proxy = getenv("X509_USER_PROXY");
         if (pu.type == "https" && x509_proxy && 0 == access(x509_proxy, R_OK)) {
             // grid proxy file holds both the certificate (chain) and key
             _debug && std::cerr << "using client certificate: " << x509_proxy << "\n";
             cli.reset(new httplib::Client(base, x509_proxy, x509_proxy));
         } else {
             cli.reset(new httplib::Client(base));
         }
         if (!cli->is_valid()) {
             throw(WebAPIException(url, ": Unable to setup http client (bad client certificate?)"));
         }

         // callers have already encoded urls with encode() ...
         cli->set_path_encode(false);
         cli->set_follow_location(false);
         cli->set_socket_options([](socket_t sock) {
             // turn on keepalive, so we know if we lose the other end...
             int optval = 1;
             setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
         });

         if (remaining > 0) {
             cli->set_max_timeout(remaining);
             cli->set_connection_timeout(remaining / 1000, (remaining % 1000) * 1000);
         }

         if (pu.type == "https") {
             // check against the grid CA's as well as the system ones
             std::string cadir(getenv("X509_CERT_DIR") ? getenv("X509_CERT_DIR") : "/etc/grid-security/certificates");
             if (0 == access(cadir.c_str(), R_OK|X_OK)) {
                 cli->set_ca_cert_path("", cadir);
             }
             cli->enable_system_ca(true);
             if (getenv("IFDH_WEB_NO_VERIFY")) {
                 _debug && std::cerr << "IFDH_WEB_NO_VERIFY set, not verifying server certificate\n";
                 cli->enable_server_certificate_verification(false);
             }
             if (getenv("https_proxy")) {
                 std::string phost;
                 int pport;
                 split_proxy(getenv("https_proxy"), phost, pport);
                 _debug && std::cerr << "using https proxy: " << phost << ":" << pport << "\n";
                 cli->set_proxy(phost, pport);
             }
         } else {
             std::string proxy(http_proxy);
             if (proxy == "" && getenv("http_proxy")) {
                 proxy = getenv("http_proxy");
             }
             if (proxy != "") {
                 std::string phost;
                 int pport;
                 split_proxy(proxy, phost, pport);
                 _debug && std::cerr << "using http proxy: " << phost << ":" << pport << "\n";
                 cli->set_proxy(phost, pport);
             }
         }

         headers.clear();
         headers.emplace("From", user + "@" + hostbuf);
         headers.emplace("User-Agent", std::string("WebAPI/") + IFDH_VERSION + "/Experiment/" + getexperiment());
         if (pu.type == "https" && strcasecmp(auth_name.c_str(), "Authorization") != 0 && 0 != (tok = get_bearer_token())) {
             headers.emplace("Authorization", std::string("Bearer ") + tok);
         }
         if (!auth_name.empty()) {
             headers.emplace(auth_name, auth_value);
         }

         if (_debug) {
             std::cerr << "sending: " << (postflag ? "POST " : "GET ") << base << pu.path << "\n";
             for (auto &h : headers) {
                 std::cerr << "sending header: " << h.first << ": " << (strcasecmp(h.first.c_str(), "Authorization") ? h.second : h.second.substr(0,16) + "...") << "\n";
             }
             std::cerr.flush();
         }

         httplib::Result res;
         if (postflag) {
             const char *content_type;
             if ( postflag == 1) {
                 content_type = "application/x-www-form-urlencoded";
             } else if ( postflag == 2) {
                 content_type = "application/json";
             } else {
                 content_type = "text/plain";
             }
             _debug && std::cerr << "sending header: Content-Type: " << content_type << "\n";
             _debug && std::cerr << "sending post data: " << postdata << "\n" << "length: " << postdata.length() << "\n";
             res = cli->Post(pu.path, headers, postdata, content_type);
         } else {
             res = cli->Get(pu.path, headers);
         }

         if (!res) {
             httplib::Error err = res.error();
             std::string msg = ": FetchError: " + httplib::to_string(err);

             _debug && std::cerr << "request failed: " << httplib::to_string(err) << "\n";
             lasterr = " (last error: " + httplib::to_string(err) + ")";

             switch (err) {
             case httplib::Error::SSLLoadingCerts:
             case httplib::Error::SSLServerVerification:
             case httplib::Error::SSLServerHostnameVerification:
                 // retrying will not help these
                 throw(WebAPIException(url, msg));
             case httplib::Error::ConnectionTimeout:
             case httplib::Error::Timeout:
                 if ( _timeout > 0 ) {
                     throw(WebAPIException(url, ": Timeout exceeded"));
                 }
                 break;
             default:
                 break;
             }
	     _debug && std::cerr << " connect failed , waiting ...\n";
             _debug && std::cerr.flush();
	     nap(5 << retries);
             continue;
         }

         _status = res->status;
         body = std::move(res->body);
	 _debug && std::cerr << "http status: " << _status << std::endl;

         _rcv_headers.clear();
         for (auto &h : res->headers) {
             _debug && std::cerr << "got header line " << h.first << ": " << h.second << "\n";
             _rcv_headers.insert(std::pair<std::string, std::string>(h.first, h.second));
         }

         retryafter = atol(res->get_header_value("Retry-After").c_str());

         if (_status == 202 && retryafter > 0) {
            nap(retryafter);
            retries--;          // it doesnt count if they told us to...
            continue;
         }

         if (_status >= 500) {
            lasterr = " (last error: HTTP status " + std::to_string(_status) + ")";
            if (_debug) {
	        std::cerr << "50x error:\n=-=-=-=-=-=-=-=-=-=\n";
                std::cerr << body;
	        std::cerr << "\n=-=-=-=-=-=-=-=-=-=\nwaiting ...";
                std::cerr.flush();
            }
            nap(random() % (5 << retries));
            continue;
         }

         if (_status >= 301 && _status <= 309 && res->has_header("Location")) {
            url = redirect_url(pu, res->get_header_value("Location"));
	    _debug && std::cerr << "Location header: url: " << url <<  "\n";
            if (_status == 303) {
               //redirected, but to a GET...
               postdata = "";
               postflag = 0;
            }
            continue;
         }

         break;
     }

     if (_status <  200 || _status >  209) {
        std::stringstream message;
        message << "\nHTTP-Status: " << _status << "\n";
        message << "Error text is:\n";
        message << body << "\n";
        message << "\n-----\n";
        _debug && std::cerr << "throwing exception, message: " << message.str() << "\n";
        throw(WebAPIException(url,message.str()));
     }

     _fromsite.str(body);
}

int
WebAPI::getStatus() {
   return _status;
}

WebAPI::~WebAPI() {
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
   ds.data().close();
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
   dsp.data().close();

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
