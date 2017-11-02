#ifndef WEB_API_H
#define WEB_API_H 1

#include <fstream>
#include <string>
#include <sstream>
#include <ext/stdio_filebuf.h>
#include <stdexcept>

namespace ifdh_util_ns {

class WebAPIException : public std::logic_error {

public:
   WebAPIException( std::string message, std::string tag) throw();
   virtual ~WebAPIException() throw() {;};
   //virtual const char *what () const throw ();
};

class WebAPI {
    std::fstream _tosite, _fromsite;
    int _tositefd, _fromsitefd;
    __gnu_cxx::stdio_filebuf<char> *_buf_in;
    int _status;
    long int _pid;
    int _timeout; // timeout for web actions as per poll()
public:
    static int _debug;
    WebAPI(std::string url, int postflag = 0, std::string postdata = "", int maxretries = 10, int timeout = -1) throw(WebAPIException);
    ~WebAPI();
    int getStatus();
    std::fstream &data() { return _fromsite; }

    static std::string encode(std::string);

    struct parsed_url {
	 std::string type;
	 std::string host;
	 int port;
	 std::string path;
    };
    static parsed_url parseurl(std::string url) throw(WebAPIException);

};

}
using namespace ifdh_util_ns;
#endif //WEB_API_H
