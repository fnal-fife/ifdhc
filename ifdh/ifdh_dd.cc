
// ifdh data dispatcher client
//


#include "utils.h"
#include "JSON.h"
#include "WebAPI.h"
#include "ifdh.h"
#include <fcntl.h>
#include <unistd.h>
#include <pwd.h>
#include <iostream>

// URLs for "hypot" experiment:
// ============================
// DATA_DISPATCHER_URL=https://metacat.fnal.gov:9443/hypot_dd/data;
// METACAT_SERVER_URL=https://metacat.fnal.gov:9443/hypot_meta_dev/app;
// DATA_DISPATCHER_AUTH_URL=https://metacat.fnal.gov:8143/auth/hypot_dev;
// METACAT_AUTH_SERVER_URL=https://metacat.fnal.gov:8143/auth/hypot_dev;
//

std::string
get_metacat_url() {
    std::string exp(getexperiment());
    std::string url("https://metacat.fnal.gov:9443/");
    url.reserve(64);
    if (getenv("METACAT_SERVER_URL")) {
        url = getenv("METACAT_SERVER_URL");
    } else {
        url += exp;
        url += "_meta_dev/app";
    }
    return url;
}

std::string
get_dd_url() {
    std::string url("https://metacat.fnal.gov:9443/");
    url.reserve(64);
    if (getenv("DATA_DISPATCHER_URL")) {
        url = getenv("DATA_DISPATCHER_URL");
    } else {
        std::string exp(getexperiment());
        url += exp;
        url += "_dd/app";
    }
    return url;
}

std::string
get_metacat_dd_auth_url() {
    std::string exp(getexperiment());
    std::string url("https://metacat.fnal.gov:8143/auth/");
    url.reserve(64);
    if (getenv("METACAT_AUTH_SERVER_URL")) {
        url = getenv("METACAT_AUTH_SERVER_URL");
    } else {
        url += exp;
        url += "_dev";
    }
    return url;
}

// This is an attempt to render the original python api code in C++
//

std::string
ifdh::new_worker_id(std::string new_id, std::string worker_id_file) {
    if (worker_id_file.empty())
       worker_id_file = ".data_dispatcher_worker_id";
    if (new_id.empty())
       new_id = unique_string();
    int fd = open(worker_id_file.c_str(), O_WRONLY);
    write(fd,new_id.c_str(), new_id.length());
    close(fd);
    return new_id;
}

void
ifdh::dd_mc_authenticate() {
    std::string user;

    if (getenv("GRID_USER"))
       user = getenv("GRID_USER");
    else if (getenv("USER"))
       user = getenv("USER");
    else
       user = "unknown_user";

    std::ifstream tfd(getToken());
    std::string token;
    std::getline(tfd, token);
    std::string username;
    tfd.close();

    WebAPI wa1(get_metacat_dd_auth_url()+"/auth?method=token&username="+user);

    _dd_mc_session_tok = wa1._rcv_headers["X-Authentication-Token"];
}

json *
ifdh::dd_create_project( 
     std::vector<std::string> files,
     std::map<std::string, std::string> common_attributes,
     std::map<std::string, std::string> project_attributes,
     std::string query,
     int worker_timeout,
     int idle_timeout,
     std::vector<std::string> users,
     std::vector<std::string> roles)  
{
     std::map<std::string, json *> msj;
     json *res;

     msj.insert( std::pair<std::string,json *>( "files", new json(files)));
     msj.insert( std::pair<std::string,json *>( "common_attributes", new json(common_attributes)));
     msj.insert( std::pair<std::string,json *>( "project_attributes", new json(project_attributes)));
     msj.insert( std::pair<std::string,json *>( "query", new json(query)));
     msj.insert( std::pair<std::string,json *>( "worker_timeout", new json(worker_timeout)));
     msj.insert( std::pair<std::string,json *>( "idle_timeout", new json(idle_timeout)));
     msj.insert( std::pair<std::string,json *>( "users", new json(users)));
     msj.insert( std::pair<std::string,json *>( "roles", new json(roles)));
     json *qj = new json(msj);
     WebAPI *wa = new WebAPI(get_dd_url()+"/create_project", 1, qj->dumps());
     res = load_json(wa->data());
     delete wa;
     delete qj;
     return res;
}


#ifdef UNITTEST
int
main() {
   WebAPI::_debug = 1;
   unsigned int watchme;
   watchme = 0xdeadbeef;
   if (watchme != 0xdeadbeef) { std::cout << "ouch1!\n"; }
   setenv("EXPERIMENT","hypot",1);
   setenv("IFDH_TOKEN_ENABLE","1",1);
   setenv("IFDH_DEBUG","2",1);

   if (watchme != 0xdeadbeef) { std::cout << "ouch1!\n"; }
   ifdh *handle = new ifdh();
   if (watchme != 0xdeadbeef) { std::cout << "ouch1!\n"; }
   std::cout << "calling dd_mc_auithenticate()\n"; std::cout.flush();
   handle->dd_mc_authenticate();
   if (watchme != 0xdeadbeef) { std::cout << "ouch1!\n"; }
   delete handle;
}
#endif
