
// ifdh data dispatcher client
//


#include "util/JSON.h"
#include "util/WebAPI.h"
#include "ifdh.h"

// This is an attempt to render the original python api code in C++
//

std::string
new_worker_id(std::string new_id, std::string worker_id_file) {
    if (!worker_id_file)
       worker_id_file = ".data_dispatcher_worker_id";
    if (!new_id)
       new_id = unique_string();
    fd = open(worker_id_file, "w");
    fd.write(new_id);
    fd.close()
    return new_id;
}

std::string
dd_uri() {
    std:;string res;
    if (getenv("DATA_DISPATCHER_URL")) {
        res = getenv("DATA_DISPATCHER_URL");
    }
    return res;
}


json *
ifdh::dd_create_project( 
     std::vector<std::str> files,
     std::map<std::str, std::str> common_attributes,
     std::map<std::str, std::str> project_attributes,
     std::str query,
     int worker_timeout,
     int idle_timeout,
     std::vector<std::str> users,
     std::vector<std::str> roles)  
{
     std::map<std::string, json *> msj;

     msj.insert( std::pair<std::str,json *>( "files", new json(files)));
     msj.insert( std::pair<std::str,json *>( "common_attributes", new json(common_attributes)));
     msj.insert( std::pair<std::str,json *>( "project_attributes", new json(project_attributes)));
     msj.insert( std::pair<std::str,json *>( "query", new json(query)));
     msj.insert( std::pair<std::str,json *>( "worker_timeout", new json(worker_timeout)));
     msj.insert( std::pair<std::str,json *>( "idle_timeout", new json(idle_timeout)));
     msj.insert( std::pair<std::str,json *>( "users", new json(users)));
     msj.insert( std::pair<std::str,json *>( "roles", new json(roles)));
     qj = new json(msj);
     wa = new WebAPI(dd_uri()+"/create_project", 1, qj->dumps());
     res = json_load(wa->data());
     delete wa;
     delete qj;
}


