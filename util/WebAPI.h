#ifndef WEB_API_H
#define WEB_API_H 1

#include <fstream>
#include <string>
#include <sstream>
#include <stdexcept>

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
    std::fstream _tosite, _fromsite;
    int _tositefd, _fromsitefd;
    int _status;
    long int _pid;
    int _timeout; // timeout for web actions as per poll()

#if __cplusplus >= 201103L
    static std::mutex _fd_mutex;
#endif
    void sockattach( std::fstream &fstr,  int &sitefd, int s, std::fstream::openmode mode);

public:
    static int _debug;
    WebAPI(std::string url, int postflag = 0, std::string postdata = "", int maxretries = 10, int timeout = -1, std::string http_proxy = ""); // throw(WebAPIException)
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
    static parsed_url parseurl(std::string url, std::string http_proxy = ""); // throw(WebAPIException)

};

}
using namespace ifdh_util_ns;
#endif //WEB_API_H
