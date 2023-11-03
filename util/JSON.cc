#include "JSON.h"
#include <sstream>
#include <stdexcept>

// small implementation of subset of JSON for C++
//  see: https://www.json.org/json-en.html
//
//  Features
//  * supports parsing/generating to/from std::iostream or std::string objects
//  * supports obj[key] and list[n] subscripting for access/modification
//  * throws domain_error for mismatches
//
// Subset is Currently missing: 
//  * booleans (true, false)
//  * \uabcd unicode escapes
//  * uses C++ iostreams operator>>(double&) to parse numbers, 
//    so may have small differences from spec.
//    see json_num::load()


// ====================
// json operations just indirect through the shared pointer..
json::json ()
{
    pval = std::shared_ptr < json_null > ();
}

json::json(const char *pc) {
    pval = std::shared_ptr<json_storage> (new json_str(std::string(pc)));
}
json::json(double n) {
    pval = std::shared_ptr<json_storage> (new json_num(n));
}

json::json (json_storage * v)
{
    pval = std::shared_ptr < json_storage > (v);
}

json::json (const json & j)
{
    pval = j.pval;
}

void
json::dump (std::ostream & os) const 
{
    pval->dump (os);
}

std::vector<json>
json::keys() {
    return (*pval).keys();
}

json & json::operator[](int i) {
    return (*pval)[i];
}

json & json::operator[](json j) {
    return (*pval)[j];
}

json::operator std::string() {
    return (std::string)(*pval);
}
json::operator double() {
    return (double)(*pval);
}
json::operator bool() {
    return (bool)(*pval);
}
json::operator int() {
    return (int)(double)(*pval);
}

// ====================

// empty constructors

json_null::json_null ()
{;
}

json_str::json_str ()
{;
}

json_str::~json_str ()
{;
}

json_bool::json_bool ()
{;
}

json_bool::~json_bool ()
{;
}

json_num::json_num ()
{;
}

json_num::~json_num ()
{;
}

json_list::json_list ()
{;
}

json_dict::json_dict ()
{;
}

// need some operator < stuff so maps work...
// do we need both of these?!?
// basically convert to strings and compare  
// could be faster another way, but this is reliable.
bool json::operator < (const json j)
{
    return dumps () < j.dumps ();
}

bool
operator < (const json j1, const json j2)
{
    return j1.dumps () < j2.dumps ();
}

// eat white space and possibly match a token
// useful string parsing unit.
static void
eats (std::istream & is, const char *match = 0)
{
    int c = is.peek ();

    // first eat whitespace...
    while (c == ' ' || c == '\n' || c== '\r' || c == '\t') {
       
#       ifdef PARSEDEBUG
        std::cerr << "eats: '" << (char)c << "'\n";
#       endif
	is.get ();
	c = is.peek ();
    }
    // then match something specific, if requested
    if (match) {
	while (*match && c == *match) {
	    is.get ();
#           ifdef PARSEDEBUG
            std::cerr << "eats: '" << (char)c << "'\n";
#           endif
	    c = is.peek ();
	    match++;
	}
	if (*match) {
	    std::stringstream ss;
	    ss << "Unexpected char '" << (char)c << "'; expected '" << *match <<
		"'.";
	    throw
	    std::domain_error (ss.str ());
	}
    }
}

void
json_null::dump (std::ostream & os) const
{
    os << "null";
}

json json_null::load (std::istream & is)
{
    json_null *jn = new json_null;
    eats (is, "null");
    return json (jn);
}

void
json_str::dump (std::ostream & os) const
{
    std::string o_val = val;
    size_t pos;
    const char *repl;
    pos = o_val.find_first_of("\\\n\r\t\f");
    while (pos != std::string::npos) {
        // NOTE not handling \uabcd...
        
        if (o_val[pos] == '\\') repl = "\\\\";
        if (o_val[pos] == '\n') repl = "\\n";
        if (o_val[pos] == '\t') repl = "\\t";
        if (o_val[pos] == '\r') repl = "\\r";
        if (o_val[pos] == '\f') repl = "\\f";

        o_val.replace(pos, 1, repl, 2);

        pos = o_val.find_first_of("\\\n\r\t\f");
    }
    os << '"' << o_val << '"';
}

json json_str::load (std::istream & is)
{
    json_str *
	jn = new json_str;
    eats (is, "\"");
    while (is.peek () != '"') {
	int
	    c = is.get ();
	if (c == '\\') {
	    c = is.get ();
            if (c == 'f') c = '\f';
            if (c == 'n') c = '\n';
            if (c == 'r') c = '\r';
            if (c == 't') c = '\t';
	    // NOTE not handling \uabcd...
	}
	jn->val.push_back (c);
    }
    eats (is, "\"");
#   ifdef PARSEDEBUG
    std::cerr << "json string: '" << jn->val << "'\n";
    std::cerr.flush();
#   endif
    return json (jn);
}

json_str::operator  std::string ()
{
    return val;
}

void
json_num::dump (std::ostream & os) const
{
    os << val;
}

json json_num::load (std::istream & is)
{
    json_num *jn = new json_num;
    is >> jn->val;
#   ifdef PARSEDEBUG
    std::cerr << "json_num: '" << jn->val << "'\n";
#   endif
    return json (jn);
}

json_num::operator double ()
{
    return val;
}
json_bool::operator bool ()
{
    return val;
}

void
json_bool::dump (std::ostream & os) const
{
    os << (val ? "true" : "false");
}

json json_bool::load (std::istream & is)
{
    json_bool *jn = new json_bool;
    if (is.peek() == 't') {
        eats(is, "true");
        jn->val = true;
    } else {
        eats(is, "false");
        jn->val = false;
    }
    return json (jn);
}

void
json_list::dump (std::ostream & os) const
{
    bool first = true;
    os << "[";
    for (auto p = val.begin (); p != val.end (); p++) {
	if (!first)
	    os << ", ";
	first = false;
	p->dump (os);
    }
    os << "]";
}

json json_list::load (std::istream & is)
{
    json_list * jl = new json_list;
    bool first = true;

    eats (is, "[");
    eats (is);
    while (is.peek () != ']') {
	if (!first) {
	    eats (is, ",");
	}
	first = false;
	jl->val.push_back (json::load (is));
    }
    eats (is, "]");
    return json (jl);
}

void
json_dict::dump (std::ostream & os) const
{
    bool first = true;
    os << "{";
    for (auto p = val.begin (); p != val.end (); p++) {
	if (!first)
	    os << ", ";
	first = false;
	p->first.dump (os);
	os << ": ";
	p->second.dump (os);
    }
    os << "}";
}

json 
json_dict::load (std::istream & is)
{
    json_dict * jd = new json_dict;
    json k, v;
    bool first = true;

    eats (is, "{");
    eats (is);
    while (is.peek () != '}') {
	if (!first) {
	    eats (is, ",");
	}
	first = false;
	k = json::load (is);
	eats (is, ":");
	v = json::load (is);
	eats (is);
	jd->val.insert (std::pair < json, json > (k, v));
    }
    eats (is, "}");
    return json (jd);
}

json & 
json_storage::operator[](int) 
{
    throw std::domain_error ("Wrong Component for [int]");
}

std::vector<json>
json_storage::keys() 
{
    throw std::domain_error ("Wrong Component for keys()");
}

void
json_storage::dump (std::ostream & os) const
{
    os.flush ();
    throw std::domain_error ("Wrong Component for dump()");
}

// top level load operation; use first non-blank character to
// redirect to subclass parser.
json json::load (std::istream & is)
{
    eats (is);
    switch (is.peek ()) {
    case '0': case '1': case '2': case '3': case '4': 
    case '5': case '6': case '7': case '8': case '9':
	return json_num::load(is);
    case '"':
	return json_str::load(is);
    case '[':
	return json_list::load(is);
    case '{':
	return json_dict::load(is);
    case 'n':
	return json_null::load(is);
    case 't': case 'f':
        return json_bool::load(is);
    default:
	std::stringstream ss;
	ss << "Unexpected char '" << is.peek () << "'.";
	throw
	std::domain_error (ss.str ());
    }
}

json json::loads (std::string s)
{
    std::stringstream ss (s.c_str ());
    return load (ss);
}


std::string json::dumps () const
{
    std::stringstream ss;
    dump (ss);
    return ss.str ();
}

json_storage::operator  double ()
{
    throw std::domain_error ("Wrong Component for double()");
}
json_storage::operator  bool ()
{
    throw std::domain_error ("Wrong Component for double()");
}

json_storage::operator  std::string ()
{
    throw std::domain_error ("Wrong Component for string()");
}

json_list::json_list (std::vector < json > v)
{
    for (auto p = v.begin (); p != v.end (); p++) {
	val.push_back (*p);
    }
}

json_list::json_list (std::vector < std::string > v)
{
    for (auto p = v.begin (); p != v.end (); p++) {
	json_str *sv = new json_str (*p);
	val.push_back (json (sv));
    }
}

json_num::json_num (double n)
{
    val = n;
}
json_bool::json_bool (bool n)
{
    val = n;
}

json_str::json_str (std::string s)
{
    val = s;
}

json_dict::json_dict (std::map < std::string, json > m)
{
    for (auto p = m.begin (); p != m.end (); p++) {
	json_str *sv = new json_str (p->first);
	val.insert (std::pair < json, json > (json (sv), p->second));
    }
}

json_dict::json_dict (std::map < std::string, std::string > m)
{
    for (auto p = m.begin (); p != m.end (); p++) {
	val.insert (std::pair < json, json > (
                       json (new json_str (p->first)),
		       json (new json_str (p->second))));
    }
}

json_dict::~json_dict ()
{
}

std::vector<json>
json_dict::keys() 
{
    std::vector<json> res;
    for (auto p = val.begin (); p != val.end (); p++) {
        res.push_back(p->first);
    }
    return res;
}

json & json_storage::operator[](json j) {
    j.dump (std::cerr);
    throw
    std::domain_error ("Wrong Component for []");
}

json & json_list::operator[](int i) {
    return val[i];
}

json & json_dict::operator[](json j) {
    return val[j];
}

#ifdef UNITTEST
int
main ()
{

    std::string t0 ("11");
    std::string tn ("null");
    std::string ta ("\"foo\"");
    std::string t1 ("[1, 22, 333]");
    std::string t2 ("{\"a\": 1, \"bb\":22, \"ccc\": 333}");
    std::string t3 ("{\"a\": \"x\", \"bb\":\"y\" , \"ccc\": 333}");
    std::string t4 ("{\"handle\": {\"project_id\": 231, \"namespace\": \"mengel\", \"name\": \"b.fcl\", \"state\": \"reserved\", \"worker_id\": \"ff9f9f85-e3af-4380-a46f-71ff9e74c56e\", \"attempts\": 1, \"attributes\": {}, \"reserved_since\": 1699037171.101709, \"replicas\": {\"FNAL_DCACHE\": {\"name\": \"b.fcl\", \"namespace\": \"mengel\", \"path\": \"/pnfs/fnal.gov/usr/hypot/rucio/mengel/9b/78/b.fcl\", \"url\": \"https://fndcadoor.fnal.gov:2880/pnfs/fnal.gov/usr/hypot/rucio/mengel/9b/78/b.fcl\", \"rse\": \"FNAL_DCACHE\", \"preference\": 100, \"available\": true, \"rse_available\": true}}, \"project_attributes\": {}}, \"reason\": \"reserved\", \"retry\": false}");

    json r;

    r = json::loads (ta);
    std::cout << "ta: " << ta << "\n";
    std::cout << "taout: ";
    r.dump (std::cout);
    std::cout << "\n";
    if (r.is_null()) std::cout << "is_null\n";
    if (r.is_dict()) std::cout << "is_dict\n";
    if (r.is_list()) std::cout << "is_list\n";
    if (r.is_number()) std::cout << "is_number\n";
    if (r.is_string()) std::cout << "is_string\n";

    r = json::loads (tn);
    std::cout << "tn: " << tn << "\n";
    std::cout << "tnout: ";
    r.dump (std::cout);
    std::cout << "\n";
    if (r.is_null()) std::cout << "is_null\n";
    if (r.is_dict()) std::cout << "is_dict\n";
    if (r.is_list()) std::cout << "is_list\n";
    if (r.is_number()) std::cout << "is_number\n";
    if (r.is_string()) std::cout << "is_string\n";

    r = json::loads (t0);
    std::cout << "t0: " << t0 << "\n";
    std::cout << "t0out: ";
    r.dump (std::cout);
    std::cout << "\n";
    if (r.is_null()) std::cout << "is_null\n";
    if (r.is_dict()) std::cout << "is_dict\n";
    if (r.is_list()) std::cout << "is_list\n";
    if (r.is_number()) std::cout << "is_number\n";
    if (r.is_string()) std::cout << "is_string\n";

    r = json::loads (t1);
    std::cout << "t1: " << t1 << "\n";
    std::cout << "t1out: ";
    r.dump (std::cout);
    std::cout << "\n";
    if (r.is_null()) std::cout << "is_null\n";
    if (r.is_dict()) std::cout << "is_dict\n";
    if (r.is_list()) std::cout << "is_list\n";
    if (r.is_number()) std::cout << "is_number\n";
    if (r.is_string()) std::cout << "is_string\n";

    r = json::loads (t2);
    std::cout << "t2: " << t2 << "\n";
    std::cout << "t2out: ";
    r.dump (std::cout);
    std::cout << "\n";
    if (r.is_null()) std::cout << "is_null\n";
    if (r.is_dict()) std::cout << "is_dict\n";
    if (r.is_list()) std::cout << "is_list\n";
    if (r.is_number()) std::cout << "is_number\n";
    if (r.is_string()) std::cout << "is_string\n";
    
    std::vector<json> kl = r.keys();
    for (auto p = kl.begin(); p != kl.end(); p++ ) {
        std::cout << "key: " << (std::string)(*p) << "\n";
    }

    r = json::loads (t3);
    std::cout << "t3: " << t3 << "\n";
    std::cout << "t3out: ";
    r.dump (std::cout);
    std::cout << "\n";

    r = json::loads (t4);
    std::cout << "t4: " << t4 << "\n";
    std::cout << "t4out: ";
    r.dump (std::cout);
    std::cout << "\n";


    std::cout << "from std::vector<std::string>\n";

    std::vector < std::string > vs;
    vs.push_back ("s0");
    vs.push_back ("s1");
    vs.push_back ("s2");
    r = json (new json_list (vs));
    r.dump (std::cout);

    std::cout << "from std::map<std::string, std::string>\n";
    std::map < std::string, std::string > mss;
    mss.insert (std::pair < std::string, std::string > ("k0", "v0"));
    mss.insert (std::pair < std::string, std::string > ("k1", "v1"));
    mss.insert (std::pair < std::string, std::string > ("k2", "v2"));
    r = json (new json_dict (mss));
    r.dump (std::cout);
}
#endif
