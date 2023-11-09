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
#include <string.h>

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

namespace ifdh_ns {

extern void get_grid_credentials_if_needed();

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
            url += "_dd/data";
        } else if ( exp == "dune" ) {
            url += "/dd/app";
        } else {
            url += "_dd_prod/data";
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
        } else {
           _dd_worker_id = new_id;
           write_file = true;
        }
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
    

    get_grid_credentials_if_needed();

    std::ifstream tfd(getToken());
    std::string token;
    std::getline(tfd, token);
    std::string username;
    tfd.close();

    WebAPI wa1(get_metacat_dd_auth_url()+"/auth?method=token&username="+user);

    _dd_mc_session_tok = wa1._rcv_headers["X-Authentication-Token"];
}

json
ifdh::metacat_query( std::string query, bool meta, bool provenance ) {
    json res;
    std::string url(get_metacat_url());
    std::string data, line;
    std::string sep, rsep;

    url += "/data/query?with_meta=" ;
    url += (meta ? "yes" : "no");
    url += "&with_provenance=";
    url +=  (provenance ? "yes" : "no");

     if (_dd_mc_session_tok == "" ) {
         dd_mc_authenticate();
     }
    std::string auth_header("X-Authentication-Token: ");
    auth_header += _dd_mc_session_tok; 

    WebAPI wa1(url, 1, query, 3, -1, "", auth_header);
    
    res = json::load(wa1.data());
    wa1.data().close();
    return res;
}

json 
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
     std::map<std::string, json > msj, mfdict;
     std::vector<json> mfinfo;
     std::vector<std::string> fsplit;
     json res, jmfinfo;
    
     if (_dd_mc_session_tok == "" ) {
         dd_mc_authenticate();
     }
     std::string auth_header("X-Authentication-Token: ");
     auth_header += _dd_mc_session_tok; 

     if (files.empty()) {
         if (!query.empty()) {
             // query metacat for files matching query
             jmfinfo = metacat_query( query, false, false );
         } else {
             jmfinfo = json(new json_null);
         }
     } else {
         for (auto finfo = files.begin(); finfo != files.end(); finfo++) {
             fsplit = split(*finfo,':');
             mfdict["namespace"] = json(new json_str(fsplit[0]));
             mfdict["name"] = json(new json_str(fsplit[1]));
             mfdict["attributes"] = json(new json_dict(common_attributes));
             mfinfo.push_back(json(new json_dict(mfdict)));
         }
         jmfinfo =json(new json_list(mfinfo));
     }

     msj.insert( std::pair<std::string,json >( "files", jmfinfo ));
     msj.insert( std::pair<std::string,json >( "project_attributes", json(new json_dict(project_attributes))));
     msj.insert( std::pair<std::string,json >( "query", json(new json_str(query))));
     msj.insert( std::pair<std::string,json >( "worker_timeout", json(new json_num(worker_timeout))));
     msj.insert( std::pair<std::string,json >( "idle_timeout", json(new json_num(idle_timeout))));
     msj.insert( std::pair<std::string,json >( "users", json(new json_list(users))));
     msj.insert( std::pair<std::string,json >( "roles", json(new json_list(roles))));
     json qj(new json_dict(msj));
     WebAPI wa(get_dd_url()+"/create_project", 1, qj.dumps(),  3, -1, "", auth_header);
     res = json::load(wa.data());
     return res;
}

char *
p_itoa(int n) {
    static char projbuf[128];
    memset(projbuf, 0, 128);
    sprintf(projbuf, "%d", n);
    return projbuf;
}

json
ifdh::dd_next_file_json(int project_id, std::string cpu_site, std::string worker_id, long int timeout, int stagger) {
    json res;

    if (stagger) {
       sleep(stagger * rand() / RAND_MAX);
    }

    worker_id = dd_worker_id(worker_id);
    std::string url = get_dd_url() + 
         "/next_file?project_id=" + p_itoa(project_id) + 
         "&worker_id=" + worker_id;

    if(!cpu_site.empty()) {
        url += "&cpu_site=";
        url += cpu_site;
    }

    bool retry = true;
 
    if ( timeout ) {
        timeout += time(0);
    }
    WebAPI::_debug = 1;

    if (_dd_mc_session_tok == "" ) {
        dd_mc_authenticate();
    }
    std::string auth_header("X-Authentication-Token: ");
    auth_header += _dd_mc_session_tok; 

    try {
        while ( retry ) {
            WebAPI wa(url, 0, "",  3, -1, "", auth_header);

            if ( wa.getStatus() == 200 ) {
                 res = json::load(wa.data());
                 retry = false;
                 wa.data().close();

                 if ((bool)res[json("retry")]) {
                     retry = true;
                     sleep(60);
                 }
            }
         
            if ( timeout ) {
                retry = time(0) < timeout;
            } 

        }
        return res;

    } catch (WebAPIException const & we) {
        if ( strstr(we.what(), "Inactive project. State=done") ) {
            // data dispatcher gives a 400 with the above when you run out..
            return json(new json_null);
        } else {
            throw we;
        }
    }
}

std::string
ifdh::dd_next_file_url(int project_id, std::string cpu_site, std::string worker_id, long int timeout, int stagger) {
    json info = dd_next_file_json(project_id, cpu_site, worker_id, timeout, stagger);


    // at this point we have json info like:
    // ...and we want the url of the first (best?) replica
    // {
    //   "handle": {
    //     "attempts": 1,
    //     "attributes": {},
    //     "name": "a.fcl",
    //     "namespace": "mengel",
    //     "project_attributes": {},
    //     "project_id": 205,
    //     "replicas": {
    //       "FNAL_DCACHE": {
    //         "available": true,
    //         "name": "a.fcl",
    //         "namespace": "mengel",
    //         "path": "/pnfs/fnal.gov/usr/hypot/rucio/mengel/e1/64/a.fcl",
    //         "preference": 100,
    //         "rse": "FNAL_DCACHE",
    //         "rse_available": true,
    //         "url": "https://fndcadoor.fnal.gov:2880/pnfs/fnal.gov/usr/hypot/rucio/mengel/e1/64/a.fcl"
    //       }
    //     },
    //     "reserved_since": 1698870000,
    //     "state": "reserved",
    //     "worker_id": "dbbc0bd4-ed2c-4684-a361-21080bdd5d42"
    //   },
    //   "reason": "reserved",
    //   "retry": false
    // }
    _debug && std::cerr << "looking for replica in:";
    if(_debug) info.dump(std::cerr);
    _debug && std::cerr << "\n";

    if (info.is_null()) { 

        // no files left...
        return "";

    } else {

        info = info[json("handle")][json("replicas")];

        json which = info.keys()[0]; // i.e. "FNAL_DCACHE" above...
        std::string name = info[which][json("name")];
        std::string ns = info[which][json("namespace")];

        _last_file_did = ns + ":" +  name;

        info = info[which][json("url")];
        return info;
    }
}

json
ifdh::dd_get_project(int project_id, bool with_files, bool with_replicas) {
    std::string url = get_dd_url() + 
                     "project?project_id=" + p_itoa(project_id)  + 
                     "&with_files=" + (with_files ? "yes" : "no") + 
                     "&with_replicass=" + (with_replicas ? "yes" : "no");

    if (_dd_mc_session_tok == "" ) {
        dd_mc_authenticate();
    }
    std::string auth_header("X-Authentication-Token: ");
    auth_header += _dd_mc_session_tok; 

    WebAPI wa(url, 0, "",  10, -1, "", auth_header);
    json res;

    if ( wa.getStatus() == 200 ) {
         res = json::load(wa.data());
    } else {
         res = json(new json_null);
    }
    return res;
}

json
ifdh::dd_file_done(int project_id, std::string file_did) {

    if (file_did == "") {
        file_did = _last_file_did;
    }

    if (_dd_mc_session_tok == "" ) {
        dd_mc_authenticate();
    }

    std::string url = get_dd_url() + "/release?handle_id=" + p_itoa(project_id)  + ":" + file_did + "&failed=no";
    std::string auth_header("X-Authentication-Token: ");
    auth_header += _dd_mc_session_tok; 

    WebAPI wa(url, 0, "",  2, -1, "", auth_header);
    json res;

    if ( wa.getStatus() == 200 ) {
         res = json::load(wa.data());
    } else {
         res = json(new json_null);
    }
    return res;
}

json 
ifdh::dd_file_failed(int project_id, std::string file_did, bool retry) {

    if (file_did == "") {
        file_did = _last_file_did;
    }
    if (_dd_mc_session_tok == "" ) {
        dd_mc_authenticate();
    }

    std::string url = get_dd_url() + 
             "/release?handle_id=" + p_itoa(project_id)  + ":" + file_did + 
             "&failed=yes&retry=" + (retry ? "yes" : "no");
    std::string auth_header("X-Authentication-Token: ");
    auth_header += _dd_mc_session_tok; 
    WebAPI wa(url, 0, "",  2, -1, "", auth_header);
    json res;

    if ( wa.getStatus() == 200 ) {
         res = json::load(wa.data());
    } else {
         res = json(new json_null);
    }
    return res;
}

}

#ifdef UNITTEST
int
main() {
   json res;
   std::string wid;
   WebAPI::_debug = 1;
   setenv("EXPERIMENT","hypot",1);
   setenv("IFDH_TOKEN_ENABLE","1",1);
   setenv("IFDH_DEBUG","2",1);

   ifdh *handle = new ifdh();
   handle->_debug = 1;
   std::cout << "calling dd_mc_auithenticate()\n"; std::cout.flush();
   handle->dd_mc_authenticate();
   wid = handle->dd_worker_id();
   std::vector<std::string> emptyvec;
   std::map<std::string, std::string> emptymap;
   std::vector<std::string> uservec;
   uservec.push_back(getenv("USER"));
   std::cout << "calling dd_create_project()\n"; std::cout.flush();
   res = handle->dd_create_project( emptyvec,  emptymap, emptymap, "files from mengel:gen_cfg", 0, 0, uservec, emptyvec);
   res.dump(std::cout);
   std::cout << "\n";
   int project_id = res[json("project_id")]; 
   std::cout << "project_id: " << project_id << "\n";

   std::string worker_id = handle-> dd_worker_id();
   std::string file_url = handle->dd_next_file_url(project_id, "bel-kwinih.fnal.gov", worker_id, 0, 0);
   
   std::cout << "file_url: " << file_url << "\n";

   handle->dd_file_done(project_id, "");

   file_url = handle->dd_next_file_url(project_id, "bel-kwinih.fnal.gov", worker_id, 0, 0);
   while (!file_url.empty()) {
   
       std::cout << "file_url: " << file_url << "\n";

       handle->dd_file_done(project_id, "");

       file_url = handle->dd_next_file_url(project_id, "bel-kwinih.fnal.gov", worker_id, 0, 0);
   }

   delete handle;
}
#endif
