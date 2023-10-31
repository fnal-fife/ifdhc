#include "JSON.h"
#include <sstream>
#include <stdexcept>

class json;
class json_none;
class json_str;
class json_num;
class json_list;
class json_dict;

// json type just indirects through the pointer..
json::json ()
{
    pval = std::shared_ptr < json_none > ();
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

json & json::operator[](int i) {
    return (*pval)[i];
}

json & json::operator[](json j) {
    return (*pval)[j];
}

// empty constructors

json_none::json_none ()
{;
}

json_str::json_str ()
{;
}

json_str::~json_str ()
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
bool json::operator < (const json j)
{
    return dumps () < j.dumps ();
}

bool
operator < (const json j1, const json j2)
{
    return j1.dumps () < j2.dumps ();
}

static void
eats (std::istream & is, const char *match = 0)
{
    int
	c = is.peek ();
    // first eat whitespace...
    while (c == ' ' || c == '\n' || c == '\t') {
	is.get ();
	c = is.peek ();
    }
    // then match something specific, if requested
    if (match) {
	while (*match && c == *match) {
	    is.get ();
	    c = is.peek ();
	    match++;
	}
	if (*match) {
	    std::stringstream ss;
	    ss << "Unexpected char '" << c << "'; expected '" << *match <<
		"'.";
	    throw
	    std::domain_error (ss.str ());
	}
    }
}

void
json_none::dump (std::ostream & os) const
{
    os << "None";
}

json json_none::load (std::istream & is)
{
    json_none *
	jn = new json_none;
    eats (is, "None");
    return json (jn);
}

void
json_str::dump (std::ostream & os) const
{
    // XXX handle \n \t etc.
    os << '"' << val << '"';
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
	    // XXX handle \n \t etc.
	}
	jn->val.push_back (c);
    }
    eats (is, "\"");
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
    return json (jn);
}

json_num::operator  double ()
{
    return val;
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

json json_dict::load (std::istream & is)
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

json & json_storage::operator[](int) {
    throw std::domain_error ("Wrong Component for [int]");
}

void
json_storage::dump (std::ostream & os) const
{
    os.flush ();
    throw std::domain_error ("Wrong Component for dump()");
}

//void json_storage::load(std::istream &is) const  { is.flush(); throw std::domain_error("Wrong Component for load()"); }

json json::load (std::istream & is)
{
    eats (is);
    switch (is.peek ()) {
    case '0': case '1': case '2': case '3': case '4': 
    case '5': case '6': case '7': case '8': case '9':
	return json_num::load (is);
    case '"':
	return json_str::load (is);
    case '[':
	return json_list::load (is);
    case '{':
	return json_dict::load (is);
    case 'N':
	return json_none::load (is);
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
	val.insert (std::pair < json,
		    json > (json (new json_str (p->first)),
			    json (new json_str (p->second))));
    }
}

json_dict::~json_dict ()
{
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
    std::string ta ("\"foo\"");
    std::string t1 ("[1, 22, 333]");
    std::string t2 ("{\"a\": 1, \"bb\":22, \"ccc\": 333}");
    std::string t3 ("{\"a\": \"x\", \"bb\":\"y\" , \"ccc\": 333}");
    json r;

    r = json::loads (ta);
    std::cout << "ta: " << ta << "\n";
    std::cout << "taout: ";
    r.dump (std::cout);
    std::cout << "\n";

    r = json::loads (t0);
    std::cout << "t0: " << t0 << "\n";
    std::cout << "t0out: ";
    r.dump (std::cout);
    std::cout << "\n";

    r = json::loads (t1);
    std::cout << "t1: " << t1 << "\n";
    std::cout << "t1out: ";
    r.dump (std::cout);
    std::cout << "\n";

    r = json::loads (t2);
    std::cout << "t2: " << t2 << "\n";
    std::cout << "t2out: ";
    r.dump (std::cout);
    std::cout << "\n";

    r = json::loads (t3);
    std::cout << "t3: " << t3 << "\n";
    std::cout << "t3out: ";
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
