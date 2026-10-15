#ifndef WEB_API_H
#define WEB_API_H 1

#include <fstream>
#include <string>
#include <sstream>
#include <stdexcept>
#include <map>
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <httplib.h>

#if __cplusplus >= 201103L
#include <thread>
#include <mutex>
#endif

namespace ifdh_util_ns {

class WebAPIException : public std::logic_error {

public:
   WebAPIException( std::string message, std::string tag); // throw()
   virtual ~WebAPIException() throw() {;};
   //virtual const char *what () const throw ();
};

class WebAPI {
    std::istringstream _data;
    int _status;
    int _timeout; // timeout for web actions as per poll()

public:
    static int _debug;
    std::map<std::string, std::string> _rcv_headers;
    WebAPI(std::string url, int postflag = 0, std::string postdata = "", int maxretries = 10, int timeout = -1, std::string http_proxy = "", std::string auth_header=""); // throw(WebAPIException)
    ~WebAPI();
    int getStatus();
    std::istringstream &data() { return _data; }

    static std::string encode(std::string);
};

}
using namespace ifdh_util_ns;
#endif //WEB_API_H
