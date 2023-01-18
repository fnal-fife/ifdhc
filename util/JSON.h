

class JSON {
public:
    class skipstream {
    public:
        virtual bool eof() = 0;
        virtual int getc() = 0;
        virtual void ungetc() = 0;
        char getnonblank();
    };
    class stringskipstream : public skipstream {
        std::string::iterator _p, _e;
    public:
        stringskipstream(std::string &s);
        bool eof();
        int getc();
        void ungetc();
    };
    class streamskipstream : public skipstream {
        std::istream &_s;
    public:
        streamskipstream(std::istream s);
        bool eof();
        int getc();
        void ungetc();
    };

    public:
    class ParseError : public std::exception {
        public:
        ParseError(skipstream &ss);
        const char *what();
    };

    class JSONdata;
    class JSONcmp;

    typedef std::map<JSONdata, JSONdata, JSONcmp> JSONmap;
    typedef std::vector<JSONdata> JSONvec;

    class JSONdata {
         public: 
         union {
            float floatval;
            const char *strvalue;
            JSONvec *list;
            JSONmap *map;
         }u;
         enum { fval, sval, lval, mval, nval} which;
         void dump(std::ostream &s) const;
         //~JSONdata();
         //JSONdata(const JSONdata &);
         JSONdata();
    };

    struct JSONcmp {
    bool operator ()(const JSONdata &x,const  JSONdata &y);
    };

    const char *getstr(skipstream &dataobj, char quote);

    JSONvec *getvec(skipstream &dataobj);

    JSONmap *getmap(skipstream &dataobj);

   JSONdata parsejson(skipstream &dataobj);
};
