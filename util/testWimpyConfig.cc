#include "WimpyConfigParser.h"
#include <iostream>
extern "C" int exit(int);

using namespace ifdh_util_ns;

main() {
   WimpyConfigParser cp;
   cp.read("test.ini");
   std::cout << "global have x?" << cp.has_option("global", "x") << std::endl;
   std::cout << "global have y?" << cp.has_option("global", "y") << std::endl;
   std::cout << "global's y?" << cp.get("global", "y") << std::endl;
   std::cout << "test have x?" << cp.has_option("test", "x") << std::endl;
   std::cout << "test have y?" << cp.has_option("test", "y") << std::endl;
   std::cout << "test's x?" << cp.get("test", "x") << std::endl;
}
