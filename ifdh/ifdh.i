%module ifdh 

%include "std_string.i"
%include "std_vector.i"
%include "std_pair.i"
%include "std_map.i"

%{
#define SWIGH_PYTHON_EXTRA_NATIVE_CONTAINERS
%}
namespace std {
    %template(vectors)   vector<string>;
    %template()          pair<string,long>;
    %template(vectorpsl) vector<pair<string,long> >;
    %template(mapsvs)    map<string,vector<string> >;
};

%exception { 
    try {
        $action
    } catch (ifdh_util_ns::WebAPIException &e) {
        e.what();
        SWIG_exception(SWIG_RuntimeError, e.what());
    } catch (std::runtime_error &e) {
        e.what();
        SWIG_exception(SWIG_IOError, e.what());
    } catch (std::logic_error &e) {
        e.what();
        SWIG_exception(SWIG_RuntimeError, e.what());
    } catch (...) {
        SWIG_exception(SWIG_RuntimeError, "unknown exception");
    }
}

%{
#define SWIG_FILE_WITH_INIT
#include "ifdh.h"
#include <malloc.h>
%}

%include "ifdh.h"

