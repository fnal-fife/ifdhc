
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
#include <fstream>
#include <stdlib.h>

// URLs for "hypot" experiment:
// ============================
// export DATA_DISPATCHER_URL=https://metacat.fnal.gov:9443/hypot_dd/data;
// export METACAT_SERVER_URL=https://metacat.fnal.gov:9443/hypot_meta_dev/app;
// export DATA_DISPATCHER_AUTH_URL=https://metacat.fnal.gov:8143/auth/hypot_dev;
// export METACAT_AUTH_SERVER_URL=https://metacat.fnal.gov:8143/auth/hypot_dev;
// For mu2e:
// export METACAT_SERVER_URL=http://dbweb5.fnal.gov:9094/mu2e_meta_prod/app
// export METACAT_AUTH_SERVER_URL=https://metacat.fnal.gov:8143/auth/mu2e
// export DATA_DISPATCHER_URL=https://metacat.fnal.gov:9443/mu2e_dd_prod/data
// export DATA_DISPATCHER_AUTH_URL=https://metacat.fnal.gov:8143/auth/mu2e
// rucio.cfg
// [client]
// rucio_host = https://hypot-rucio.fnal.gov
// auth_host = https://auth-hypot-rucio.fnal.gov

std::string
get_metacat_url() {
    std::string exp(getexperiment());
    std::string url("https://metacat.fnal.gov:9443/");
    url.reserve(64);
    if (getenv("METACAT_SERVER_URL")) {
        url = getenv("METACAT_SERVER_URL");
    } else {
        url += exp;
        if ( exp == "hypot" ) {
            url += "_meta_dev/app";
        } else if ( exp == "dune" )  {
            url += "_meta_demo/app";
        } else {
            url += "_meta_prod/app";
        }
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
        if ( exp == "hypot" ) {
            url += "_dd/app";
        } else if ( exp == "dune" ) {
            url += "/dd/app";
        } else {
            url += "_dd_prod/data"
        }
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
        if ( exp == "hypot" ) {
            url += "_dev";
        }
    }
    return url;
}

// This is an attempt to render the original python api code in C++
//

std::string
ifdh::dd_worker_id(std::string new_id, std::string worker_id_file) {
    bool write_file = false; // only write the cache file if we're setting id
    if (_dd_worker_id.empty()) {
        if (worker_id_file.empty())
           worker_id_file = ".data_dispatcher_worker_id";
         
        if (new_id.empty()) {
           if (access(worker_id_file.c_str(), O_RDONLY)) {
               std::ifstream wif(worker_id_file);
               std::getline(wif, _dd_worker_id);
               wif.close();
           } else {
               _dd_worker_id = unique_string();
               write_file = true;
           }
        }
    } else {
       _dd_worker_id = new_id;
       write_file = true;
    }
    if ( write_file) {
       int fd = open(worker_id_file.c_str(), O_WRONLY);
       write(fd,new_id.c_str(), new_id.length());
       close(fd);
    }
    return _dd_worker_id;
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
     std::map<std::string, json *> msj, mfdict;
     std::vector<json *> mfinfo;
     std::vector<std::string> fsplit;
     json *res;

     for (auto finfo = files.begin(); finfo != files.end(); finfo++) {
         fsplit = split(*finfo,':');
         mfdict["namespace"] = new json(fsplit[0]);
         mfdict["name"] = new json(fsplit[1]);
         mfdict["attributes"] = new json(common_attributes);
         mfinfo.push_back(new json(mfdict));
     }

     msj.insert( std::pair<std::string,json *>( "files", new json(mfinfo)));
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

json *
ifdh::dd_next_file_json(std::string project_id, std::string cpu_site, std::string worker_id, time_t timeout, int stagger) {
    std::string info;

    if (stagger) {
       sleep(stagger * rand() / RAND_MAX);
    }

    worker_id = dd_worker_id(worker_id);
    std::string url = get_dd_url() + 
         "/next_file?project_id=" + project_id + 
         "&worker_id=" + worker_id;

    if(!cpu_site.empty()) {
        url += "&cpu_site=";
        url += cpu_site;
    }

    bool retry = true;
 
    if ( timeout ) {
        timeout += time(0);
    }

    while ( retry ) {
        WebAPI *wa = new WebAPI(url, 0, 0);

        if ( wa->getStatus() == 200 ) {
             res = load_json(wa->data());
             retry = false;
        } else {
            sleep(60);
        }
     
        if ( timeout ) {
            retry = time(0) < timeout;
        }

        delete wa;
    }
    return info; 
}

std::string
ifdh::dd_next_file_url(std::string project_id, std::string cpu_site, std::string worker_id, time_t timeout, int stagger) {
    json *info = dd_next_file_json(project_id, cpu_site, worker_id, timeout, stagger);
    // at this point we have json info like:
    // 
    // {
    //   "attempts": 1,
    //   "attributes": {},
    //   "name": "a.fcl",
    //   "namespace": "mengel",
    //   "project_attributes": {},
    //   "project_id": 188,
    //   "replicas": [
    //     {
    //       "available": true,
    //       "name": "a.fcl",
    //       "namespace": "mengel",
    //       "path": "/pnfs/fnal.gov/usr/hypot/rucio/mengel/e1/64/a.fcl",
    //       "preference": 100,
    //       "rse": "FNAL_DCACHE",
    //       "rse_available": true,
    //       "url": "https://fndcadoor.fnal.gov:2880/pnfs/fnal.gov/usr/hypot/rucio/mengel/e1/64/a.fcl"
    //     }
    //   ],
    //   "reserved_since": 1697747190.818226,
    //   "state": "reserved",
    //   "worker_id": "4157a5e3"
    // }
    //
    // ...and we want the url of the first (best?) replica
    return (*(*(*info)["replicas"])[0])["url"].sval();
}

json *
ifdh::dd_get_project(int project_id, boolean with_files, boolean with_replicas) {
    std::string url = get_dd_url() + "project?project_id=" + itoa(project_id) + 
                     "&with_files=" + (with_files ? "yes" : "no") + 
                     "&with_replicass=" + (with_replicas ? "yes" : "no");
    WebAPI *wa = new WebAPI(url, 0, 0);

    if ( wa->getStatus() == 200 ) {
         res = load_json(wa->data());
    } else {
         res = json_None();
    }
    return res;
}

json *
ifdh::dd_file_done(int project_id, std::string file_did) {
    std::string url = get_dd_url() + "release?handle_id=" + itoa(project_id) + ":" + file_did + "&failed=no";
    WebAPI *wa = new WebAPI(url, 0, 0);

    if ( wa->getStatus() == 200 ) {
         res = load_json(wa->data());
    } else {
         res = json_None();
    }
    return res;
}

json *
ifdh::dd_file_failed(int project_id, std::string file_did) {
    std::string url = get_dd_url() + "release?handle_id=" + itoa(project_id) + ":" + file_did + "&failed=yes";
    WebAPI *wa = new WebAPI(url, 0, 0);

    if ( wa->getStatus() == 200 ) {
         res = load_json(wa->data());
    } else {
         res = json_None();
    }
    return res;
}

#ifdef UNITTEST
int
main() {
   WebAPI::_debug = 1;
   setenv("EXPERIMENT","hypot",1);
   setenv("IFDH_TOKEN_ENABLE","1",1);
   setenv("IFDH_DEBUG","2",1);

   ifdh *handle = new ifdh();
   std::cout << "calling dd_mc_auithenticate()\n"; std::cout.flush();
   handle->dd_mc_authenticate();
   delete handle;
}
#endif
