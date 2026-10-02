#ifndef WEB_API_H
#define WEB_API_H 1

#include <fstream>
#include <string>
#include <sstream>
#include <stdexcept>
#include <map>

namespace ifdh_util_ns {

class WebAPIException : public std::logic_error {

public:
   WebAPIException( std::string message, std::string tag); // throw()
   virtual ~WebAPIException() throw() {;};
   //virtual const char *what () const throw ();
};

//
// The response body is read completely into memory, and handed
// back as a stringstream.  close() is a no-op kept so code written
// for the older fstream based interface still compiles.
//
class WebAPIStream : public std::stringstream {
public:
    void close() {;}
};

class WebAPI {
    WebAPIStream _fromsite;
    int _status;
    int _timeout; // overall timeout for web actions in milliseconds

public:
    static int _debug;
    std::map<std::string, std::string> _rcv_headers;
    WebAPI(std::string url, int postflag = 0, std::string postdata = "", int maxretries = 10, int timeout = -1, std::string http_proxy = "", std::string auth_header=""); // throw(WebAPIException)
    ~WebAPI();
    int getStatus();
    WebAPIStream &data() { return _fromsite; }

    static std::string encode(std::string);

    struct parsed_url {
	 std::string type;
	 std::string host;
	 int port;
	 std::string path;
    };
    static parsed_url parseurl(std::string url, std::string http_proxy = ""); // throw(WebAPIException)

};

}
using namespace ifdh_util_ns;
#endif //WEB_API_H
