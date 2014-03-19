#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "ifbeam.h"
#include "../util/utils.h"
#include "../util/WebAPI.h"
#include <math.h>
#include <sys/types.h>
#include <unistd.h>
#include "../util/ifdh_version.h"

namespace ifbeam_ns {

int BeamFolder::_debug;

static int
round_down_to_nearest(int n, int modulus) {
    if (n % modulus == 0) {
       return n;
    } else {
       return n - n % modulus;
    }
}

static int
round_up_to_nearest(int n, int modulus) {
    if (n % modulus == 0) {
       return n;
    } else {
       return n - n % modulus + modulus;
    }
}


BeamFolder::BeamFolder(std::string bundle_name, std::string url, double time_width) {
    // round to "standardized" time_width for cacheability
    if (time_width <= 600) {
        time_width = round_up_to_nearest((int)time_width, 60);
    } else if ( time_width <= 2700 ) {
        time_width = round_up_to_nearest((int)time_width, 900);
    } else {
        time_width = round_up_to_nearest((int)time_width, 3600);
    }
    _time_width = time_width;
    _bundle_name =  bundle_name;
    _url = url.length() > 0 ? url : "http://ifb-data.fnal.gov:8089/ifbeam";
    _cache_start = 0.0;
    _cache_end = 0.0;
    _cache_slot = -1;
    _valid_window = 120.0;
    _epsilon = .125;
#ifndef OLD_CACHE
    _values = 0;
    _cur_row = 0;
    _cur_row_num = -1;

    // pass info about us down for UserAgent: string
    std::stringstream uabuf;
    uabuf << "ifdh/" << IFDH_VERSION << "/Experiment/ " << getexperiment();
    setUserAgent( (char *) uabuf.str().c_str() );
#endif
}

BeamFolder::~BeamFolder() {
#ifndef OLD_CACHE
    if (_values) {
        releaseDataset(_values);
    }
    if (_cur_row) {
       releaseTuple(_cur_row);
    }
#else
    ;
#endif
}


void 
BeamFolder::setValidWindow(double w) {
   _valid_window = w;
}

double 
BeamFolder::getValidWindow() {
   return _valid_window;
}

#ifdef OLD_CACHE
void
BeamFolder::FillCache(double when) throw(WebAPIException) {
    std::vector<std::string>::iterator it;
    std::string st;

    if (when >= _cache_start && when < _cache_end ) {
        // we're already in the cache...
        return;
    }

    // round to standard windows...
    if (_time_width <= 600) 
        when = round_down_to_nearest((int)when,60);
    else if (_time_width <= 2700)
        when = round_down_to_nearest((int)when,900);
    else 
        when = round_down_to_nearest((int)when,3600);

    // cache is flushed...
    _values.clear();
    // we average 15 lines/second so preallocate some space
    _values.reserve(int(_time_width*15));
    _n_values = 0;
    _cache_slot = -1;

    std::stringstream varurl;
    
    varurl << _url << "/data/data" 
           << "?b=" << _bundle_name
           << std::setiosflags(std::ios::fixed) 
           << "&t0=" << when
           << "&t1=" << when + _time_width;

    _debug && std::cout << "fetching url: " << varurl.str() << "\n";

    WebAPI s(varurl.str());

    getline(s.data(), st); // header...

    _debug && std::cout << "got line: " << st << "\n";

    while( !s.data().eof() && !s.data().fail() ) {
	getline(s.data(), st);

        _debug && std::cout << "got line: " << st << "\n";

	_values.push_back(st);
        _n_values++;
    }
    if ( s.data().fail() && !s.data().eof() ) {
        throw(WebAPIException("ios.h fail()  error reading data from server",""));
    }
    if (_n_values == 0) {
        throw(WebAPIException("No data values returned from server for URL: ",varurl.str()));
    }
    _cache_start = when;
    _cache_end = when + _time_width;
}

double 
BeamFolder::slot_time(int n) {
   return atof(_values[n].c_str())/1000.0;
}

std::string 
BeamFolder::slot_var(int n) {
     std::string s = _values[n];
     size_t p1, p2;
     p1 = s.find(',');
     p2 = s.find(',',p1+1);
     return s.substr(p1+1,p2-p1-1);
}

double 
BeamFolder::slot_value(int n, int j) {
     std::string s = _values[n];
     int i;
     size_t p1=0,p2=0;
     p2=0;
     for( i = 0; i < j+4 ; i++ ) {
         p1 = p2;
         p2 = s.find(',',p1+1); 
     }
     if (p1 == std::string::npos) {
          throw(WebAPIException("not that many values in vector",""));
     }
     return atof(s.substr(p1+1,p2-p1).c_str());
}

#else

void
BeamFolder::FillCache(double when) throw(WebAPIException) {
    int err = 0;

    // round to standard windows...
    if (_time_width <= 600) 
        when = round_down_to_nearest((int)when,60);
    else if (_time_width <= 2700)
        when = round_down_to_nearest((int)when,900);
    else 
        when = round_down_to_nearest((int)when,3600);

    time_t t0 = (time_t) when;
    time_t t1 = (time_t) (when + _time_width);

    if (when >= _cache_start && when < _cache_end ) {
        // we're already in the cache...
        return;
    }

    if (_values) {
        releaseDataset(_values);
    }


    //
    // retries are now done in getBundleForInterval
    //
    _values = getBundleForInterval((_url + "/data").c_str(), _bundle_name.c_str(), t0, t1, &err);

    int status = getHTTPstatus(_values);

    if (status != 200) {
       char ebuf[80];
       sprintf(ebuf, "HTTP error: status: %d:", status);
       throw(WebAPIException(ebuf, getHTTPmessage(_values)));
    }

    if (!_values && err)  throw(WebAPIException("getBundleForInterval", strerror(err)));
    _cache_start = when;
    _cache_end = when + _time_width;
    _n_values = getNtuples(_values) - 1;

    if (_n_values <= 0 ) {
         std::stringstream tbuf; 
         tbuf << std::setw(9) << when << " url: " <<  _url;
         throw(WebAPIException("No ifbeam data available for this time: ", tbuf.str() ));
    }
    
    // look for a values column, default to 3
    
    _values_column = 3;

    static char buf[512];
    Tuple t = cachedGetTuple(0);
    for (int i = 0; i < 10; i++ ) {
        getStringValue(t,i,buf,512,&err);
        if (0 == strncmp(buf,"value",5)) {
            _values_column = i;
            break;
        }
    }
}

Tuple
BeamFolder::cachedGetTuple(int n) {
   if (n != _cur_row_num) {
       if (_cur_row) {
	  releaseTuple(_cur_row);
       }
       _cur_row = getTuple(_values, n);
       _cur_row_num = n;
   }
   return _cur_row;
}

double 
BeamFolder::slot_time(int n) {
   int err = 0;
   double res;
   Tuple t = cachedGetTuple( n+1);
   if (!t) throw(WebAPIException("slot_time"," getTuple"));
   res = getDoubleValue(t,0,&err)/1000.0;
   if (err)  throw(WebAPIException("slot_time", strerror(err)));
   //releaseTuple(t);
   return res;
}

std::string 
BeamFolder::slot_var(int n) {
   int err = 0;
   static char buf[512];
   Tuple t = cachedGetTuple( n+1);
   if (!t) throw(WebAPIException("slot_var"," getTuple"));
   getStringValue(t,1,buf,512,&err);
   if (err)  throw(WebAPIException("slot_var", strerror(err)));
   std::string res(buf);
   //releaseTuple(t);
   return res;
}

double 
BeamFolder::slot_value(int n, int j) {
   int err = 0;
   double res;
   Tuple t = cachedGetTuple( n+1);
   if (!t) throw(WebAPIException("slot_value","getTuple"));
   res = getDoubleValue(t, j+_values_column, &err);
   if (err)  throw(WebAPIException("getDoubleVal", strerror(err)));
   //releaseTuple(t);
   return res;
}

#endif

void
BeamFolder::set_epsilon( double e ) {
  _epsilon = e;
}

int 
BeamFolder::time_eq(double x, double y) { return fabs(x -  y) <= _epsilon ; }

void
BeamFolder::find_first(int &first_time_slot, double &first_time, double when) {

    if (_cache_slot >= 0 && time_eq(_cache_slot_time,when) ) {

        _debug && std::cout << "finding cached slot: " << _cache_slot << "\n";

        first_time_slot = _cache_slot;
        first_time = slot_time(first_time_slot);

    } else {

	// start proportionally through the data and search for the time boundary
	// nearest the time we are looking for

	first_time_slot = int(_n_values * (when - _cache_start) / (_cache_end - _cache_start));

        if (first_time_slot > _n_values - 1)
            first_time_slot = _n_values - 1;

        if (first_time_slot < 0)
            first_time_slot = 0;

	while( first_time_slot < _n_values-1 && slot_time(first_time_slot) < when ) {
	   first_time_slot++;
	}
	while( first_time_slot > 0 && slot_time(first_time_slot) > when ) {
	   first_time_slot--;
	}

        // pick the closer time
        if (first_time_slot < _n_values-1 && 
            fabs(slot_time(first_time_slot) - when) > fabs(slot_time(first_time_slot+1) - when) ){

            _debug && std::cout << "switching from reference time:" 
				<< slot_time(first_time_slot) << "\n";
            first_time_slot = first_time_slot+1;
        }
        first_time = slot_time(first_time_slot);

        _debug && std::cout << "picked reference time:" << first_time << "\n";
       
        // find the start of the picked time
	while( first_time_slot > 0 && time_eq(slot_time(first_time_slot-1),first_time) ) {
	   first_time_slot--;
	}

        // chache this result
        _cache_slot = first_time_slot;
        _cache_slot_time = when;
    }
}

void
BeamFolder::find_name(int &first_time_slot, double &first_time, int &search_slot, std::string curvar) {
        search_slot = first_time_slot;
 
        // scan for named variable with this time
        _debug && std::cout << "searching for var: " << curvar << "\n";
        _debug && std::cout << "checking slot: " << search_slot << " name: " << slot_var(search_slot) << "\n";
 
        if (search_slot < _n_values-1)
            _debug && std::cout << "true?" 
	    		    << time_eq(slot_time(search_slot+1),first_time) << "\n";
            _debug && std::cout << "true?" 
			    << (slot_var(search_slot) < curvar)<<  "\n";

        while( search_slot < _n_values-1 && time_eq(slot_time(search_slot+1),first_time) && slot_var(search_slot) != curvar  )  {

            search_slot++;

            _debug && std::cout << "checking slot: " << search_slot 
				<< " name: " << slot_var(search_slot) 
				<<"\n";

            _debug && std::cout << "true?" 
				<< time_eq(slot_time(search_slot+1), first_time) << "\n";

            _debug && std::cout << "true?" 
				<< (slot_var(search_slot) < curvar)<<  "\n";
         }

        _debug && std::cout << "after namesearch, search_slot is : " << search_slot 
			<< "data: " << "\n";
}
void
BeamFolder::GetNamedData(double when, std::string variable_list, ...)  throw(WebAPIException) {
    std::vector<std::string> variables, values;
    std::vector<std::string>::iterator rvit, it;
    std::string curvar;
    double *curdest;
    double *timedest = 0;
    size_t bpos;
    size_t atpos;
    va_list al;
    int first_time_slot;
    int array_slot;
    int search_slot;
    double first_time;
    bool append_time;

    _debug && std::cout << "looking for time" <<  when << "\n";
    // fetch data into cache (if needed)
    FillCache(when);

    // find first slot with this time; may have it cached...
    if (_n_values == 0) {
         throw(WebAPIException(curvar, "No data found at this time"));
    }


    find_first(first_time_slot, first_time, when);

    _debug && std::cout << "after scans, first_time_slot is : " 
                        << first_time_slot << "data: " 
                        << "\n";
   
    va_start(al, variable_list);

    variables = split(variable_list,',');
    
    for( rvit = variables.begin(); rvit != variables.end(); rvit++) {


        // find varname to lookup :
        // if we have a [n] on the end, set slot to n, and 
        // make it var[]
        //  

        bpos = rvit->find('[');
        if (bpos != std::string::npos) {
            array_slot = atoi(rvit->c_str()+bpos+1);
            curvar = *rvit;
            curvar[bpos+1] = ']';
            curvar = curvar.substr(0,bpos+2);
        } else {
            curvar =  *rvit;
            array_slot = 0;
        }

        atpos = curvar.find('@');
        if (atpos != std::string::npos) {
            curvar = curvar.substr(0,atpos) + curvar.substr(atpos+1);
            append_time = 1;
        } else {
            append_time = 0;
        }
     
        // the place to put the value is the next varargs parameter
        curdest = (double *) va_arg(al,void*);

        if (append_time) {
             timedest = (double *) va_arg(al,void*);
        }

        find_name(first_time_slot, first_time, search_slot, curvar);
        

        if ( curvar == slot_var(search_slot)) {
           std::vector <std::string> vallist;
           
           *curdest = slot_value(search_slot,array_slot);
           if (append_time) {
               *timedest = slot_time(search_slot);
           }
        } else {
            throw(WebAPIException(curvar, "-- variable not found"));
        }
    }
}

std::vector<double> 
BeamFolder::GetNamedVector(double when, std::string variable_name, double *actual_time ) throw (WebAPIException) {
    double first_time;
    int first_time_slot;
    int search_slot;
    double value;
    int i;
    std::vector<double> res;
   
    FillCache(when);

    if (_n_values == 0) {
         throw(WebAPIException(variable_name, "No data found at this time"));
    }

    find_first(first_time_slot, first_time, when);
    find_name(first_time_slot, first_time, search_slot, variable_name);

    if ( variable_name != slot_var(search_slot)) {
        throw(WebAPIException(variable_name, "-- variable not found"));
    }

    if ( fabs(slot_time(search_slot) - when) > _valid_window ) {
        throw(WebAPIException(variable_name, "-- only stale data found"));
    }

    if (actual_time != 0) {
       *actual_time = slot_time(search_slot);
    }

    //  keep looking for a value until we get an exception
    //  
    for (i = 0; i < 10000; i++ ) {
       try {
           value = slot_value(search_slot, i);   
           res.push_back(value);
       } catch (WebAPIException e) {
           break;
       }
   }
   return res;
}
 

std::vector<double> 
BeamFolder::GetTimeList() {
   std::vector<double> res;
   std::string lookfor;

   // happily assume first variable in first sample appears
   // in each sample, so find all the times for which  it appears
   lookfor = slot_var(0);
   for(int i = 0; i < _n_values; i++) {
       if (lookfor == slot_var(i)) {
           res.push_back(slot_time(i));
       }
   }
   return res;
}

// hmm...

std::vector<std::string> 
BeamFolder::GetDeviceList() {
   std::vector<std::string> res;
   std::string lookfor;

   res.push_back(lookfor);
   for(int  i = 1; i < _n_values; i++) {
        bool seen_before = false;
        for( unsigned int j = 0; j < res.size() ; j++ )
           if (res[j] == slot_var(i))
              seen_before = true;

        if (!seen_before)
            res.push_back(slot_var(i));
   }
   return res;
}


BeamFolderScanner::BeamFolderScanner(std::string bundle, double start_time) :
   BeamFolder(bundle) , cur_slot(0) {
   FillCache(start_time);
}

int
BeamFolderScanner::NextDataRow(double &time, std::string &name, std::vector<double> &values) {
   static char buf[512];
   int err;

   if (cur_slot >= _n_values) {
      FillCache(_cache_end + .001);
      cur_slot = 0;
   }
   if (cur_slot >= _n_values) {
      return 0;
   }
   Tuple t = cachedGetTuple(cur_slot++);
   time = getDoubleValue(t,0,&err)/1000.0;
   getStringValue(t,1,buf,512,&err);
   name = buf;
   values.clear();
   for(int i = _values_column; i < getNfields(t); i++ ) {
        values.push_back( getDoubleValue(t,i,&err) );
   }
   return 1;
}

}


#ifdef UNITTEST
int
main() {
    double when = 1323722800.0;
    double twhen = 1334332800.0;
    double t2when = 1334332800.4;
    double  nodatatime = 1373970148.000000;
    double ehmgpr, em121ds0, em121ds5, trtgtd;
    double t1, t2;
    WebAPI::_debug = 1;
    BeamFolder::_debug = 1;
    std::string teststr("1321032116708,E:HMGPR,TORR,687.125");

    std::cout << std::setiosflags(std::ios::fixed);
 
  // test with someone who doesn't have any data, to check error timeouts
 // std::cout << "Trying a nonexistent location\n";
 // BeamFolder bfu("NuMI_Physics_A9","http://bel-kwinith.fnal.gov/");
 // bfu.set_epsilon(.125);
//
 // try {
  //  bfu.GetNamedData(nodatatime,"E:HP121@[1]",&ehmgpr,&t1);
   // std::cout << "got values " << ehmgpr <<  "for E:HP121[1]at time " << t1 << "\n";
 // } catch (WebAPIException &we) {
  //     std::cout << "got exception:" << we.what() << "\n";
  //}

  std::cout << "Trying the default location\n";
  BeamFolder bf("NuMI_Physics_A9");
  bf.set_epsilon(.125);

  try {
    double tlist[] = {1386058021.613409,1386058023.279863,1386058079.149919,1386058080.816768};
    for (int i = 0; i < 4 ; i++) {
       bf.GetNamedData(tlist[i],"E:TRTGTD@",&trtgtd,&t1);
       std::cout << "got values " << trtgtd <<  "for E:TRTGTD at time " << t1 << "\n";
    }
  } catch (WebAPIException &we) {
       std::cout << "got exception:" << we.what() << "\n";
  }
  exit(0);
  try {
    bf.GetNamedData(nodatatime,"E:HP121@[1]",&ehmgpr,&t1);
    std::cout << "got values " << ehmgpr <<  "for E:HP121[1]at time " << t1 << "\n";
  } catch (WebAPIException &we) {
       std::cout << "got exception:" << we.what() << "\n";
  }
  try {
    bf.GetNamedData(nodatatime,"E:HP121@[1]",&ehmgpr,&t1);
    std::cout << "got values " << ehmgpr <<  "for E:HP121[1]at time " << t1 << "\n";
  } catch (WebAPIException &we) {
       std::cout << "got exception:" << we.what() << "\n";
  }

  try {
    bf.GetNamedData(t2when,"E:NOSUCHVARIABLE",&ehmgpr,&t1);
    std::cout << "got values " << ehmgpr <<  "for E:HP121[1]at time " << t1 << "\n";
  } catch (WebAPIException &we) {
       std::cout << "got exception:" << we.what() << "\n";
  }

  try {
    bf.GetNamedData(t2when,"E:HP121@[1]",&ehmgpr,&t1);
    std::cout << "got values " << ehmgpr <<  "for E:HP121[1]at time " << t1 << "\n";

    bf.GetNamedData(when,"E:TR101D@",&ehmgpr,&t1);
    std::cout << "got values " << ehmgpr <<  "for E:TR101D at time " << t1 << "\n";
    bf.GetNamedData(twhen,"E:TR101D@",&ehmgpr,&t1);
    std::cout << "got values " << ehmgpr <<  "for E:TR101D at time " << t1 << "\n";
    
    bf.GetNamedData(when,"E:HMGPR",&ehmgpr);
    bf.GetNamedData(when,"E:M121DS[0],E:M121DS[5]",&em121ds0, &em121ds5);
    std::cout << "got value " << ehmgpr << "for E:HMGPR\n";
    std::cout << "got values " << em121ds0 << ',' << em121ds5 << "for E:M121DS[0,5]\n";

    bf.GetNamedData(when,"E:M121DS@[0],E:M121DS@[5]",&em121ds0, &t1, &em121ds5, &t2);
    std::cout << "got values " << em121ds0 << ',' << em121ds5 << "for E:M121DS[0,5] at time " << t1 << "\n";
 
    std::cout << "time stamps:";
    std::vector<double> times = bf.GetTimeList();
    for (size_t i = 0; i < times.size(); i++) {
        std::cout << times[i] << ", ";
    }
    std::cout << "\n";

    std::cout << "variables:";
    std::vector<std::string> vars = bf.GetDeviceList();
    for (size_t i = 0; i < vars.size(); i++) {
        std::cout << vars[i] << ", ";
    }
    std::cout << "\n";

    std::cout << "vector E:M121DS[]:";
    std::vector<double> values = bf.GetNamedVector(when,"E:M121DS[]");
    for (size_t i = 0; i < values.size(); i++) {
        std::cout << values[i] << ", ";
    }
    std::cout << "\n";

    std::cout << "vector E:M121DS[] with time:";
    values = bf.GetNamedVector(when,"E:M121DS[]",&t1);
    std::cout << "at time: " << t1 << ": ";
    for (size_t i = 0; i < values.size(); i++) {
        std::cout << values[i] << ", ";
    }
    std::cout << "\n";
    
    bf.GetNamedData(1323726316.528,"E:HMGPR",&ehmgpr);
    std::cout << "got value " << ehmgpr << "for E:HMGPR\n";
    bf.GetNamedData(1323726316.528,"E:HMGPR@",&ehmgpr, &t1);
    std::cout << "got value " << ehmgpr << "for E:HMGPR at time " << t1 << "\n";
    bf.GetNamedData(1323726318.594,"E:HMGPR",&ehmgpr);
    std::cout << "got value " << ehmgpr << "for E:HMGPR\n";
    bf.GetNamedData(1323726318.594,"E:HMGPR@",&ehmgpr,&t2);
    std::cout << "got value " << ehmgpr << "for E:HMGPR at time " << t2 << "\n";
    std::cout << "Done!\n";
  } catch (WebAPIException &we) {
       std::cout << "got exception:" << we.what() << "\n";
  }

  
  std::string name;
  double t;
  std::vector<double> vals;
  int count = 0;

  BeamFolderScanner bfs("NuMI_Physics_A9", 1334332800.4);
  while( bfs.NextDataRow( t, name, vals ) && count++ < 100 )  {
     
    std::cout << "got time "<< t << " name " << name << "values:" ;
    for(unsigned i = 0; i < vals.size(); i++ )
       std::cout << vals[i] << ", " ;
    std::cout << "\n";

  }

}
#endif
