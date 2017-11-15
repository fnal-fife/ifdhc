#!/bin/sh

hdr=$1

xlate=false

# ignore parens and commas
IFS="(),$IFS"

get_env_names() {
    (
      # get env entries from ifdh.cfg
      grep env ../ifdh.cfg | sed -e 's/.*=//'
      # get getenv calls from ifdh code
      grep getenv *.cc ../util/*.cc | sed -e 's/.*getenv("\([A-Z_]*\)").*/\1/' | grep -v ':' 
    ) | sort -u | grep -v '^$'
}

get_ifdh_env_names() {
   get_env_names | grep '^IFDH_'
}
get_other_env_names() {
   get_env_names | grep -v '^IFDH_' | grep -v '^$'
}

get_getopt_long_list() {
   printf "struct option envopts[] =  {\n";
   get_ifdh_env_names | 
       sed -e 's/^IFDH_//' |
       tr '[A-Z]' '[a-z]' | 
       while read name
       do
           printf "{\"%s\", 1, 0, 'i'},\n" $name
       done

   get_other_env_names | 
       tr '[A-Z]' '[a-z]' | 
       while read name
       do
           printf "{\"%s\", 1, 0, 'o'},\n" $name
       done
   printf "{0,0,0,0},\n"
   printf "};\n";
}

print_env_getopt_loop() {
    printf "int argflag = 1, argind;\n"
    printf "std::string ifdh_str(\"IFDH_\"), es;\n"
    printf "while(argflag) switch(getopt_long(argc, argv, \"i:o:\",envopts, &argind)) { \n"
    printf "case 'i': es = ifdh_str + stoupper(envopts[argind].name) + \"=\" + optarg; sputenv(es);break;\n";
    printf "case 'o': es = stoupper(envopts[argind].name) + \"=\" + optarg; sputenv(es);break;\n";
    printf "default: argflag = 0; break;\n"
    printf "}\n";
}

define_stoupper() {
cat <<EOF
std::string
stoupper(std::string s) {
  for(std::string::iterator pc = s.begin(); pc != s.end(); pc++ ) {
     *pc = toupper(*pc);
  }
  return s;
}
void
sputenv(std::string s) {
   putenv(strdup(s.c_str()));
}
EOF
}

print_env_help_list() {
   printf "cout << \"Global options for all commands:\\\n\";\n"
   get_env_names | 
         while read name
         do
            topt=`echo $name | sed -e 's/^IFDH_//' | tr '[A-Z]' '[a-z]'`
            printf "cout << \"\t--%s=value\t\tset %s environment variable to value\\\\n\";\n" "$topt" "$name"
         done
}


#  some funcs have a template type that confuses things, fix it
fixargs() {
   echo "in fixargs..." >&2
   case "$args" in 
   \>*) 
       type="$type $func >"
       func="$2"
       shift
       shift
       args="$*"
       echo "setting func to $func args to $args" >&2
       ;;
   esac
}

else=""
sawclass=false
while read type func args 
do
    fixargs $args

    if [ "$type" = 'class' -a "$func" = "ifdh" ]
    then
        sawclass=true
    fi

    if $sawclass
    then
        :
    else
        continue
    fi

    docall=false
    echo "DEBUG: line " $type "||" $func "||" $args >&2

    case "$type" in
    //)
 	lastcomment="$lastcomment \\\n\\\t $func $args"
        ;;
    public:)
        lastcomment=""
        printf "// generated by h_to_main.sh -- do not edit\n\n"
	printf "#include \"$hdr\"\n\n"
	printf "#include <string.h>\n"
	printf "#include <stdlib.h>\n"
	printf "#include <iostream>\n"
	printf "#include <string>\n"
	printf "#include <utility>\n"
	printf "#include <vector>\n"
	printf "#include <stdexcept>\n"
        printf "#include <getopt.h>\n"
        printf "using namespace std;\n"
        printf "using namespace ifdh_util_ns;\n"
        # printf "extern \"C\" { void exit(int); }\n"
        printf "static void usage();\n"
        define_stoupper
        printf "static int has_args_thru(char **argv, int i) { for(int j = 0; j <= i; j++) if (!argv[j]) return 0; return 1;}\n"
        printf "static int di(int i)\t\t{ exit(i);  return 1; }\n"
        printf "static int ds(string s)\t\t{ cout << s << \"\\\\n\"; return s.size() == 0; }\n"
        printf "static int dv(vector<string> v)\t\t{ for(size_t i = 0; i < v.size(); i++) { cout << v[i] << \"\\\\n\"; } exit(v.size() == 0); }\n"
        printf "static int dvpsl(vector<pair<string, long int> > v)\t{ for(size_t i = 0; i < v.size(); i++) { cout << v[i].first << "'"\\t"'" << v[i].second << \"\\\\n\"; } exit(v.size() == 0);}\n"
        printf "static int dvmsvs(map<string,vector<string> > m)\t{for( map<string,vector<string> >:: iterator i  = m.begin(); i != m.end(); ++i) { cout << i->first << "'":\\n"'"; for (size_t j = 0; j < i->second.size(); ++j) { cout << "'"\\t"'" <<  i->second[j] << "'"\\n"'";}  } exit(m.empty()); }\n"
        printf "static vector<string> argvec(int argc, char **argv) { vector<string> v; for(int i = 0; i < argc; i++ ) { v.push_back(argv[i]); } return v; }\n"
        printf "static vector<pair<string,long> > argvecpair(int argc, char **argv) { vector<pair<string,long> > v; int i; for(i = 0; i < argc - 1; i+=2 ) { v.push_back(pair<string,long>(argv[i],atol(argv[i+1]))); } return v; }\n"
        printf "static string catargs(int argc, char **argv) { string res; for(int i = 0; i < argc; i++ ) { res.append(argv[i]); res.append(\" \"); } return res; }\n"

        printf "\n"

        printf "int\nmain(int argc, char **argv) { \n"
        printf "\tifdh i;\n"
        printf "\tif (! argv[1] || 0 == strcmp(argv[1],\"--help\") || (argv[2] && 0 == strcmp(argv[2],\"--help\"))) { \n"
        printf "\t\tusage();exit(0);\n"
        printf "\t}\n";
        get_getopt_long_list
        printf "char *argv1 = argv[1]; argv++; argc--;\n"
        print_env_getopt_loop
        printf "argv += optind - 1; argc -= optind - 1;  argv[0] = (char *)\"ifdh\"; argv[1] = argv1;\n"
        printf "\ttry {\n"
	xlate=true;
	;;
    # \}\;)
    private:)
        $xlate || continue
	printf "\telse {\nusage(); exit(1);\t\n\t}\n"
        printf "   } catch (std::exception &e) {\n"
        printf "      time_t t = time(0);"
        printf "      std::cerr << \"Exception:\" << e.what() << std::endl;\n"
        printf "      std::cerr << \"at:\" << ctime(&t) << std::endl;\n"
        printf "      exit(1);\n"
        printf "   }\n"
	printf "}\n"
	printf "void usage(){\n"
        printf "$help\n"
        print_env_help_list
	printf "}\n"
	xlate=false;
        ;;
    std::vector*std::pair*)
        pfunc="dvpsl"
        docall=true;
        ;;
    std::map*std::string*)
        pfunc="dvmsvs"
        docall=true;
        ;;
    std::vector*std::string*)
        pfunc="dv"
        docall=true;
        ;;
    void)
        lastcomment=""
	;;
    int)
        pfunc="di"
        docall=true;
        ;;       
    std::string)
        pfunc="ds"
        docall=true
        ;;
    esac

    $xlate || continue
    if $docall
    then
        echo "args is: '$args'" >&2
        case "$args" in
        std::string\ message\ *)
          echo "message special case" >&2
          cargs="catargs"
          args="args";
          ;;

        *vector*pair*)
          echo "argvecpair case: $args" >&2 
          cargs="argvecpair"
          args="args";
          ;;
        *vector*string*args*)
          echo "argvec case: $args" >&2 
          cargs="argvec"
          args="args";
          ;;
        *)
          echo "usual case: $args" >&2 
          cargs=`echo $args | perl -pe 's/std::string//g; s/= ".*?"//g; s/= 0//g; s/= -1//g; s/;.*//; s/(long )?int ([a-z]*)/atol_$2/g;'`
          args=`echo $args | perl -pe 's/std::string//g; s/= ".*?"//g; s/= 0//g; s/= -1//g; s/;.*//; s/(long )?int//g;'`
          ;;
        esac
        echo "cargs are now: $cargs" >&2
        help="$help
                cout << \"\\\\t\033[1mifdh [global-args] $func $args\033[0m\\\\n\\\\t  $lastcomment\\\n\\\n\";"
	printf "\t${else}if (argc > 1 && 0 == strcmp(argv[1],\"$func\")) $pfunc(i.$func("
        else="else "
        i=2
        sep=""
        for a in $cargs
        do 
            case "$a" in
            catargs*)
                 echo "saw catargs case" >&2
                 printf "catargs(argc-2,argv+2)"
                 ;;
            argvecpair*)
                 echo "saw argvecpair case" >&2
                 printf "argvecpair(argc-2,argv+2), (1==argc%%2)?argv[argc-1]:\"\" "
                 ;;
            argvec*)
                 echo "saw argvec case" >&2
                 printf "argvec(argc-2,argv+2)"
                 ;;
            atol*)
                 echo "saw atol case" >&2
                 printf "$sep has_args_thru(argv,$i)?atol(argv[$i]):-1"; 
                 ;;
            *)
                 printf "$sep has_args_thru(argv,$i)?argv[$i]:\"\""; 
                 ;;
            esac
	    sep="," 
	    i=$((i + 1))
	done
        printf "));\n"
 
        lastcomment=""
    fi   

done < $hdr


