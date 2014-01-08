%module ifdh 

%include "std_string.i"

%include "std_vector.i"

namespace std {
    %template(vectors) vector<string>;
};

%exception { 
    try {
        $action
    } catch (ifdh_util_ns::WebAPIException &e) {
        e.what();
        SWIG_exception(SWIG_RuntimeError, e.what());
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
%}

%include "ifdh.h"

